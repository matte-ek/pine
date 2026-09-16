#include "Editing.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "Components/Components.hpp"
#include "Components/Transform/Transform.hpp"
#include "Duplication/Duplication.hpp"
#include "History/History.hpp"
#include "Schema/Schema.hpp"
#include "Values/Values.hpp"
#include "../LevelCamera/LevelCamera.hpp"
#include "Gui/Panels/EntityList/EntityListPanel.hpp"
#include "Gui/Shared/Selection/Selection.hpp"
#include "Other/PlayHandler/PlayHandler.hpp"
#include "Other/Actions/Actions.hpp"
#include "Pine/Engine/Engine.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Pine/World/Entities/Entities.hpp"

namespace
{
    using nlohmann::json;
    using namespace Editor::DebugServer;
    namespace Values = Editing::Values;
    namespace Adapters = Editing::Components;
    namespace Duplication = Editing::Duplication;

    constexpr std::size_t MaxOperations = 128;
    constexpr std::size_t MaxBodyBytes = 256 * 1024;
    constexpr int MaxJsonDepth = 32;
    constexpr std::size_t MaxDuplicatedEntities = 1024;

    struct DuplicatedEntity
    {
        std::string SourceKey;
        std::string Key;
        std::string ParentKey;
        Duplication::EntityState State;
    };

    const json EntityProperties = {
        { "name", { { "type", "string" }, { "minLength", 1 },
            { "description", "Nonempty name without null characters; duplicates are allowed." } } },
        { "active", { { "type", "boolean" },
            { "description", "Enables this entity's components; does not change descendants or component active flags." } } },
        { "static", { { "type", "boolean" },
            { "description", "Marks this entity as static; does not change descendants." } } }
    };

    struct ComponentEdit
    {
        const Adapters::Adapter* Adapter = nullptr;
        json State;
    };

    enum class OperationType
    {
        CreateEntity,
        UpdateEntity,
        ReparentEntity,
        DeleteEntity,
        DuplicateEntity,
        AddComponent,
        UpdateComponent,
        RemoveComponent
    };

    struct Operation
    {
        OperationType Type = OperationType::CreateEntity;
        std::string Name;
        std::string Reference;
        std::string ParentReference;
        Pine::Entity* Parent = nullptr;
        Pine::Entity* Entity = nullptr;
        Pine::Component* Target = nullptr;
        json EntityPatch;
        std::vector<ComponentEdit> Components;
        std::size_t DeletedEntityCount = 0;
        std::map<Pine::ComponentType, std::size_t> FreedComponents;
        std::vector<DuplicatedEntity> DuplicatedEntities;
    };

    std::string CreationKey(const Operation& operation, const std::size_t index)
    {
        return operation.Reference.empty() ? "operation:" + std::to_string(index) : "ref:" + operation.Reference;
    }

    bool IsSceneEntity(const Pine::Entity* entity)
    {
        // Temporary editor entities and anything attached beneath them are outside this API.
        for (auto ancestor = entity; ancestor != nullptr; ancestor = ancestor->GetParent())
        {
            if (ancestor->GetTemporary())
            {
                return false;
            }
        }
        return entity != nullptr;
    }

    void RequireSceneEntity(const Pine::Entity* entity, const std::string& path)
    {
        Values::Require(entity != nullptr, path, "Entity does not exist.");
        Values::Require(IsSceneEntity(entity), path, "Cannot edit or parent under a temporary editor entity.");
    }

    void RequireRestorableComponent(const Pine::Component* component, const std::string& path)
    {
        const auto adapter = Adapters::Find(component->GetType());
        Values::Require(adapter != nullptr, path, "This component has no history restoration adapter.");
        Adapters::Prepare(*adapter, adapter->Read(nullptr), adapter->Read(component), path + "/beforeState");
    }

    Pine::Component* FindComponent(const Pine::UId id)
    {
        for (const auto entity : Pine::Entities::GetList())
        {
            for (const auto component : entity->GetComponents())
            {
                if (component->GetId() == id)
                {
                    return component;
                }
            }
        }
        return nullptr;
    }

    void PrepareParent(const json& parent, const std::string& path, Operation& operation,
        const std::set<std::string>& references)
    {
        Values::Object(parent, path, { "id", "ref" });
        Values::Require(parent.size() == 1, path, "Expected exactly one of id or ref.");

        if (parent.contains("ref"))
        {
            operation.ParentReference = Values::String(parent.at("ref"), path + "/ref");
            Values::Require(references.count(operation.ParentReference) != 0, path + "/ref",
                "Parent ref must name an earlier entity.create or entity.duplicate operation.");
        }
        else
        {
            operation.Parent = Pine::Entities::Find(Values::Id(parent.at("id"), path + "/id"));
            RequireSceneEntity(operation.Parent, path + "/id");
        }
    }

    void PrepareCreation(const json& input, const std::string& path, Operation& operation,
        std::set<std::string>& references)
    {
        Values::Object(input, path, { "op", "name", "ref", "parent", "components" }, { "op" });
        operation.Type = OperationType::CreateEntity;
        operation.Name = input.contains("name") ? Values::String(input.at("name"), path + "/name") : "Entity";

        if (input.contains("parent"))
        {
            PrepareParent(input.at("parent"), path + "/parent", operation, references);
        }

        if (input.contains("ref"))
        {
            operation.Reference = Values::String(input.at("ref"), path + "/ref");
            Values::Require(references.insert(operation.Reference).second, path + "/ref", "Duplicate entity ref.");
        }

        if (!input.contains("components"))
        {
            return;
        }

        const auto& components = input.at("components");
        Values::Require(components.is_array(), path + "/components", "Expected an array.");
        std::set<Pine::ComponentType> types;

        for (std::size_t index = 0; index < components.size(); index++)
        {
            const auto componentPath = path + "/components/" + std::to_string(index);
            const auto& component = components[index];
            Values::Object(component, componentPath, { "type", "properties" }, { "type" });
            const auto name = Values::String(component.at("type"), componentPath + "/type");
            const auto adapter = Adapters::Find(name);
            Values::Require(adapter != nullptr, componentPath + "/type", "Unsupported component type. See /edit/schema.");
            Values::Require(types.insert(adapter->Type).second, componentPath + "/type", "Duplicate component type.");

            const auto properties = component.value("properties", json::object());
            operation.Components.push_back({ adapter,
                Adapters::Prepare(*adapter, adapter->Read(nullptr), properties, componentPath + "/properties") });
        }
    }

    void PrepareEntityUpdate(const json& input, const std::string& path, Operation& operation)
    {
        Values::Object(input, path, { "op", "target", "properties" }, { "op", "target", "properties" });
        Values::Object(input.at("target"), path + "/target", { "id" }, { "id" });
        operation.Type = OperationType::UpdateEntity;
        operation.Entity = Pine::Entities::Find(Values::Id(input.at("target").at("id"), path + "/target/id"));
        RequireSceneEntity(operation.Entity, path + "/target/id");

        // These properties have no cross-field constraints. Keep only supplied fields so
        // sequential patches compose without restoring omitted values from preparation.
        operation.EntityPatch = input.at("properties");
        Values::Properties(operation.EntityPatch, EntityProperties, path + "/properties");
    }

    void ApplyEntityUpdate(Pine::Entity* entity, const json& properties)
    {
        if (properties.contains("name"))
        {
            entity->SetName(properties.at("name").get<std::string>());
        }
        if (properties.contains("active") && entity->GetActive() != properties.at("active").get<bool>())
        {
            entity->SetActive(properties.at("active").get<bool>());
            entity->SetDirty(true);
        }
        if (properties.contains("static") && entity->GetStatic() != properties.at("static").get<bool>())
        {
            entity->SetStatic(properties.at("static").get<bool>());
            entity->SetDirty(true);
        }
    }

    void PrepareReparent(const json& input, const std::string& path, Operation& operation,
        const std::set<std::string>& references)
    {
        Values::Object(input, path, { "op", "target", "parent" }, { "op", "target", "parent" });
        Values::Object(input.at("target"), path + "/target", { "id" }, { "id" });
        operation.Type = OperationType::ReparentEntity;
        operation.Entity = Pine::Entities::Find(Values::Id(input.at("target").at("id"), path + "/target/id"));
        RequireSceneEntity(operation.Entity, path + "/target/id");

        if (!input.at("parent").is_null())
        {
            PrepareParent(input.at("parent"), path + "/parent", operation, references);
        }
    }

    void PrepareDeletion(const json& input, const std::string& path, Operation& operation)
    {
        Values::Object(input, path, { "op", "target" }, { "op", "target" });
        Values::Object(input.at("target"), path + "/target", { "id" }, { "id" });
        operation.Type = OperationType::DeleteEntity;
        operation.Entity = Pine::Entities::Find(Values::Id(input.at("target").at("id"), path + "/target/id"));
        RequireSceneEntity(operation.Entity, path + "/target/id");
    }

    void ValidateSceneOperations(std::vector<Operation>& operations, int& operationIndex)
    {
        struct ProposedEntity
        {
            std::string Parent;
            bool Temporary = false;
            bool HasUnsupportedComponents = false;
            Pine::Entity* Existing = nullptr;
            std::map<Pine::ComponentType, std::size_t> PooledComponents;
            std::vector<std::string> Children;
            Duplication::EntityState State;
        };

        const bool includesDuplication = std::any_of(operations.begin(), operations.end(), [](const Operation& operation)
        {
            return operation.Type == OperationType::DuplicateEntity;
        });
        std::size_t duplicatedCount = 0;

        // Existing IDs, request refs and unnamed creations occupy separate key spaces.
        // Empty means scene root. Removing a key invalidates all later uses of it.
        std::unordered_map<std::string, ProposedEntity> entities;
        for (const auto entity : Pine::Entities::GetList())
        {
            const auto parent = entity->GetParent();
            auto& proposed = entities[entity->GetId().ToString()];
            proposed.Existing = entity;
            proposed.Parent = parent == nullptr ? "" : parent->GetId().ToString();
            proposed.Temporary = entity->GetTemporary();
            for (const auto child : entity->GetChildren())
            {
                proposed.Children.push_back(child->GetId().ToString());
            }
            if (includesDuplication)
            {
                proposed.State = Duplication::Read(entity);
            }
            for (const auto component : entity->GetComponents())
            {
                proposed.HasUnsupportedComponents |= !Duplication::Supports(component->GetType());
                if (!component->GetStandalone())
                {
                    proposed.PooledComponents[component->GetType()]++;
                }
            }
        }

        for (std::size_t index = 0; index < operations.size(); index++)
        {
            auto& operation = operations[index];
            operationIndex = static_cast<int>(index);
            const auto path = "/operations/" + std::to_string(index);

            auto owner = operation.Entity;
            if (operation.Target != nullptr)
            {
                owner = operation.Target->GetParent();
            }
            const auto target = owner == nullptr ? "" : owner->GetId().ToString();
            if (owner != nullptr)
            {
                Values::Require(entities.count(target) != 0, path + "/target/id",
                    "Entity was deleted by an earlier operation in this batch.");
            }

            std::string parent;
            if (!operation.ParentReference.empty())
            {
                parent = "ref:" + operation.ParentReference;
                Values::Require(entities.count(parent) != 0, path + "/parent/ref",
                    "Parent ref was deleted by an earlier operation in this batch.");
            }
            else if (operation.Parent != nullptr)
            {
                parent = operation.Parent->GetId().ToString();
                Values::Require(entities.count(parent) != 0, path + "/parent/id",
                    "Parent was deleted by an earlier operation in this batch.");
            }

            if (operation.Type == OperationType::CreateEntity)
            {
                const auto key = CreationKey(operation, index);
                auto& created = entities[key];
                created.Parent = parent;
                if (!parent.empty())
                {
                    entities.at(parent).Children.push_back(key);
                }
                created.PooledComponents[Pine::ComponentType::Transform] = 1;
                for (const auto& component : operation.Components)
                {
                    created.PooledComponents[component.Adapter->Type] = 1;
                }
                if (includesDuplication)
                {
                    created.State.Name = operation.Name;
                    const auto transform = Adapters::Find(Pine::ComponentType::Transform);
                    created.State.Components.push_back({ transform->Type, {}, transform->Read(nullptr) });
                    for (const auto& component : operation.Components)
                    {
                        if (component.Adapter->Type == Pine::ComponentType::Transform)
                        {
                            created.State.Components.front().Properties = component.State;
                        }
                        else
                        {
                            created.State.Components.push_back({ component.Adapter->Type, {}, component.State });
                        }
                    }
                }
            }
            else if (operation.Type == OperationType::DuplicateEntity)
            {
                // Snapshot the source tree before inserting copies, especially when duplicating
                // an ancestor that already contains an earlier duplicate from this batch.
                std::vector<std::string> sources{ target };
                std::unordered_map<std::string, std::string> copiedKeys;
                for (std::size_t node = 0; node < sources.size(); node++)
                {
                    Values::Require(++duplicatedCount <= MaxDuplicatedEntities, path + "/target/id",
                        "Batch exceeds the limit of 1024 duplicated entities.");
                    const auto sourceKey = sources[node];
                    const auto& source = entities.at(sourceKey);
                    Values::Require(!source.Temporary, path + "/target/id",
                        "Cannot duplicate a hierarchy containing a temporary editor entity.");
                    auto state = source.State;
                    Duplication::Validate(state, path + "/target/id");
                    const auto key = node == 0 ? CreationKey(operation, index)
                        : "duplicate:" + std::to_string(index) + ":" + std::to_string(node);
                    const auto parentKey = node == 0 ? source.Parent : copiedKeys.at(source.Parent);
                    copiedKeys[sourceKey] = key;
                    operation.DuplicatedEntities.push_back({ sourceKey, key, parentKey, std::move(state) });
                    sources.insert(sources.end(), source.Children.begin(), source.Children.end());
                }

                for (const auto& copied : operation.DuplicatedEntities)
                {
                    ProposedEntity proposed;
                    proposed.Parent = copied.ParentKey;
                    proposed.State = copied.State;
                    for (auto& component : proposed.State.Components)
                    {
                        component.SourceId = Pine::UId::Empty();
                        proposed.PooledComponents[component.Type]++;
                    }
                    entities.emplace(copied.Key, std::move(proposed));
                    if (!copied.ParentKey.empty())
                    {
                        entities.at(copied.ParentKey).Children.push_back(copied.Key);
                    }
                }
            }
            else if (operation.Type == OperationType::UpdateEntity && includesDuplication)
            {
                auto& state = entities.at(target).State;
                state.Name = operation.EntityPatch.value("name", state.Name);
                state.Active = operation.EntityPatch.value("active", state.Active);
                state.Static = operation.EntityPatch.value("static", state.Static);
            }
            else if (operation.Type == OperationType::ReparentEntity)
            {
                for (auto ancestor = parent; !ancestor.empty(); ancestor = entities.at(ancestor).Parent)
                {
                    Values::Require(ancestor != target, path + "/parent",
                        "Reparenting would create a hierarchy cycle.");
                }
                auto& proposed = entities.at(target);
                if (proposed.Parent != parent)
                {
                    if (!proposed.Parent.empty())
                    {
                        auto& children = entities.at(proposed.Parent).Children;
                        children.erase(std::remove(children.begin(), children.end(), target), children.end());
                    }
                    if (!parent.empty())
                    {
                        entities.at(parent).Children.push_back(target);
                    }
                    proposed.Parent = parent;
                }
            }
            else if (operation.Type == OperationType::AddComponent)
            {
                entities.at(target).PooledComponents[operation.Components.front().Adapter->Type]++;
                if (includesDuplication)
                {
                    const auto& component = operation.Components.front();
                    entities.at(target).State.Components.push_back({ component.Adapter->Type, {}, component.State });
                }
            }
            else if (operation.Type == OperationType::RemoveComponent || operation.Type == OperationType::UpdateComponent)
            {
                if (operation.Type == OperationType::RemoveComponent && !operation.Target->GetStandalone())
                {
                    entities.at(target).PooledComponents[operation.Target->GetType()]--;
                }
                if (includesDuplication)
                {
                    auto& components = entities.at(target).State.Components;
                    const auto found = std::find_if(components.begin(), components.end(), [&](const auto& component)
                    {
                        return component.SourceId == operation.Target->GetId();
                    });
                    if (operation.Type == OperationType::RemoveComponent)
                    {
                        components.erase(found);
                    }
                    else
                    {
                        found->Properties = operation.Components.front().State;
                    }
                }
            }
            else if (operation.Type == OperationType::DeleteEntity)
            {
                std::vector<std::string> deleted;
                for (const auto& [key, proposed] : entities)
                {
                    for (auto ancestor = key; !ancestor.empty(); ancestor = entities.at(ancestor).Parent)
                    {
                        if (ancestor != target)
                        {
                            continue;
                        }
                        Values::Require(!proposed.Temporary, path + "/target/id",
                            "Cannot delete a hierarchy containing a temporary editor entity.");
                        Values::Require(!proposed.HasUnsupportedComponents, path + "/target/id",
                            "Cannot undo deletion of a hierarchy containing unsupported components.");
                        if (proposed.Existing != nullptr)
                        {
                            for (const auto component : proposed.Existing->GetComponents())
                            {
                                RequireRestorableComponent(component, path + "/target");
                            }
                        }
                        deleted.push_back(key);
                        for (const auto& [type, count] : proposed.PooledComponents)
                        {
                            operation.FreedComponents[type] += count;
                        }
                        break;
                    }
                }
                operation.DeletedEntityCount = deleted.size();
                const auto previousParent = entities.at(target).Parent;
                if (!previousParent.empty())
                {
                    auto& children = entities.at(previousParent).Children;
                    children.erase(std::remove(children.begin(), children.end(), target), children.end());
                }
                for (const auto& key : deleted)
                {
                    entities.erase(key);
                }
            }
        }
    }

    void ApplyReparent(Pine::Entity* entity, Pine::Entity* parent)
    {
        const auto previous = entity->GetParent();
        if (previous == parent)
        {
            return;
        }

        // AddChild does not detach an existing parent. Update both child lists through
        // the public operations; neither operation changes the local transform.
        if (previous != nullptr)
        {
            previous->RemoveChild(entity);
        }
        if (parent != nullptr)
        {
            parent->AddChild(entity);
        }
        Adapters::Transform::MarkHierarchyDirty(entity);
    }

    void PrepareAddition(const json& input, const std::string& path, Operation& operation)
    {
        Values::Object(input, path, { "op", "target", "type", "properties" }, { "op", "target", "type" });
        Values::Object(input.at("target"), path + "/target", { "id" }, { "id" });
        operation.Type = OperationType::AddComponent;
        operation.Entity = Pine::Entities::Find(Values::Id(input.at("target").at("id"), path + "/target/id"));
        RequireSceneEntity(operation.Entity, path + "/target/id");

        const auto name = Values::String(input.at("type"), path + "/type");
        const auto adapter = Adapters::Find(name);
        Values::Require(adapter != nullptr, path + "/type", "Unsupported component type. See /edit/schema.");
        Values::Require(adapter->Type != Pine::ComponentType::Transform, path + "/type",
            "Every entity already has its required Transform. Use component.update.");
        Values::Require(adapter->AllowAddRemove, path + "/type", "This component type does not support addition.");

        // All supported additions can exist with only Transform. A RigidBody without a
        // Collider stores configuration but creates no physics actor until a Collider is present.
        Values::Require(operation.Entity->GetTransform() != nullptr, path + "/target/id",
            "Component requires the entity's Transform.");
        operation.Components.push_back({ adapter, Adapters::Prepare(*adapter, adapter->Read(nullptr),
            input.value("properties", json::object()), path + "/properties") });
    }

    void ValidateCapacity(const std::vector<Operation>& operations, int& operationIndex)
    {
        auto entityCount = Pine::Entities::GetList().size();
        const auto maximum = Pine::Engine::GetEngineConfiguration().m_MaxObjectCount;
        std::map<Pine::ComponentType, std::size_t> availableComponents;
        for (const auto adapter : Adapters::GetAdapters())
        {
            const auto& block = Pine::Components::GetData(adapter->Type);
            auto& available = availableComponents[adapter->Type];
            for (std::uint32_t index = 0; index < block.m_ComponentOccupationArraySize; index++)
            {
                if (!block.ComponentIndexValid(index))
                {
                    available++;
                }
            }
        }

        // A removal frees its slot only for subsequent operations, not earlier additions.
        for (std::size_t index = 0; index < operations.size(); index++)
        {
            operationIndex = static_cast<int>(index);
            const auto path = "/operations/" + std::to_string(index);
            const auto& operation = operations[index];
            const auto reserveComponent = [&](Pine::ComponentType type)
            {
                auto& available = availableComponents.at(type);
                Values::Require(available != 0, path,
                    "Batch exceeds the " + std::string(Adapters::Find(type)->Name) + " pool capacity.");
                available--;
            };

            if (operation.Type == OperationType::CreateEntity)
            {
                Values::Require(++entityCount <= maximum, path, "Batch exceeds the entity pool capacity.");
                reserveComponent(Pine::ComponentType::Transform);
                for (const auto& component : operation.Components)
                {
                    if (component.Adapter->Type != Pine::ComponentType::Transform)
                    {
                        reserveComponent(component.Adapter->Type);
                    }
                }
            }
            else if (operation.Type == OperationType::AddComponent)
            {
                reserveComponent(operation.Components.front().Adapter->Type);
            }
            else if (operation.Type == OperationType::DuplicateEntity)
            {
                for (const auto& copied : operation.DuplicatedEntities)
                {
                    Values::Require(++entityCount <= maximum, path, "Batch exceeds the entity pool capacity.");
                    for (const auto& component : copied.State.Components)
                    {
                        reserveComponent(component.Type);
                    }
                }
            }
            else if (operation.Type == OperationType::RemoveComponent && !operation.Target->GetStandalone())
            {
                availableComponents.at(operation.Target->GetType())++;
            }
            else if (operation.Type == OperationType::DeleteEntity)
            {
                entityCount -= operation.DeletedEntityCount;
                for (const auto& [type, count] : operation.FreedComponents)
                {
                    if (availableComponents.count(type) != 0)
                    {
                        availableComponents.at(type) += count;
                    }
                }
            }
        }
    }

    std::vector<Operation> Prepare(const json& body, int& operationIndex)
    {
        Values::Object(body, "", { "version", "operations" }, { "version", "operations" });
        Values::Require(body.at("version").is_number_integer() && body.at("version") == 1,
            "/version", "Expected editing protocol version 1.");

        const auto& inputs = body.at("operations");
        Values::Require(inputs.is_array() && !inputs.empty() && inputs.size() <= MaxOperations,
            "/operations", "Expected an array of 1 to 128 operations.");

        std::vector<Operation> operations;
        std::set<std::string> references;
        std::unordered_map<Pine::UId, json> proposedStates;
        std::unordered_set<Pine::UId> removedComponents;
        std::map<Pine::Entity*, std::map<Pine::ComponentType, std::size_t>> proposedComponentCounts;

        const auto componentCounts = [&](Pine::Entity* entity) -> auto&
        {
            auto [entry, inserted] = proposedComponentCounts.try_emplace(entity);
            if (inserted)
            {
                for (const auto component : entity->GetComponents())
                {
                    entry->second[component->GetType()]++;
                }
            }
            return entry->second;
        };

        for (std::size_t index = 0; index < inputs.size(); index++)
        {
            operationIndex = static_cast<int>(index);
            const auto path = "/operations/" + std::to_string(index);
            const auto& input = inputs[index];
            Values::Require(input.is_object() && input.contains("op"), path + "/op", "Operation name is required.");
            const auto name = Values::String(input.at("op"), path + "/op");
            Operation operation;

            if (name == "entity.create")
            {
                PrepareCreation(input, path, operation, references);
            }
            else if (name == "entity.update")
            {
                PrepareEntityUpdate(input, path, operation);
            }
            else if (name == "entity.reparent")
            {
                PrepareReparent(input, path, operation, references);
            }
            else if (name == "entity.delete")
            {
                PrepareDeletion(input, path, operation);
            }
            else if (name == "entity.duplicate")
            {
                Values::Object(input, path, { "op", "target", "ref" }, { "op", "target" });
                Values::Object(input.at("target"), path + "/target", { "id" }, { "id" });
                operation.Type = OperationType::DuplicateEntity;
                operation.Entity = Pine::Entities::Find(Values::Id(input.at("target").at("id"), path + "/target/id"));
                RequireSceneEntity(operation.Entity, path + "/target/id");
                if (input.contains("ref"))
                {
                    operation.Reference = Values::String(input.at("ref"), path + "/ref");
                    Values::Require(references.insert(operation.Reference).second, path + "/ref", "Duplicate entity ref.");
                }
            }
            else if (name == "component.add")
            {
                PrepareAddition(input, path, operation);
                auto& count = componentCounts(operation.Entity)[operation.Components.front().Adapter->Type];
                Values::Require(count == 0, path + "/type", "Entity already has this component type.");
                count++;
            }
            else if (name == "component.update" || name == "component.remove")
            {
                operation.Type = name == "component.update" ? OperationType::UpdateComponent : OperationType::RemoveComponent;
                if (operation.Type == OperationType::UpdateComponent)
                {
                    Values::Object(input, path, { "op", "target", "properties" }, { "op", "target", "properties" });
                }
                else
                {
                    Values::Object(input, path, { "op", "target" }, { "op", "target" });
                }
                Values::Object(input.at("target"), path + "/target", { "id" }, { "id" });
                const auto id = Values::Id(input.at("target").at("id"), path + "/target/id");
                Values::Require(removedComponents.count(id) == 0, path + "/target/id",
                    "Component was removed by an earlier operation in this batch.");
                operation.Target = FindComponent(id);
                Values::Require(operation.Target != nullptr, path + "/target/id", "Component does not exist.");
                RequireSceneEntity(operation.Target->GetParent(), path + "/target/id");

                const auto adapter = Adapters::Find(operation.Target->GetType());
                Values::Require(adapter != nullptr, path + "/target/id", "This component type is not writable. See /edit/schema.");

                if (operation.Type == OperationType::RemoveComponent)
                {
                    Values::Require(adapter->Type != Pine::ComponentType::Transform, path + "/target/id",
                        "Cannot remove the required Transform component.");
                    Values::Require(adapter->AllowAddRemove, path + "/target/id",
                        "This component type does not support removal.");
                    removedComponents.insert(id);
                    componentCounts(operation.Target->GetParent())[adapter->Type]--;
                }
                else
                {
                    // Later patches build on earlier proposed state without changing live objects.
                    const auto previous = proposedStates.find(id);
                    const auto state = previous == proposedStates.end() ? adapter->Read(operation.Target) : previous->second;
                    auto next = Adapters::Prepare(*adapter, state, input.at("properties"), path + "/properties");
                    proposedStates[id] = next;
                    operation.Components.push_back({ adapter, std::move(next) });
                }
            }
            else
            {
                throw Values::ValidationError(path + "/op", "Unsupported operation. See /edit/schema.");
            }

            operations.push_back(std::move(operation));
        }

        ValidateSceneOperations(operations, operationIndex);
        ValidateCapacity(operations, operationIndex);
        operationIndex = -1;
        return operations;
    }

    json DescribeEntity(const Pine::Entity* entity)
    {
        json components = json::array();
        for (const auto component : entity->GetComponents())
        {
            if (const auto adapter = Adapters::Find(component->GetType()))
            {
                components.push_back(Adapters::Describe(*adapter, component));
            }
        }

        json parent = nullptr;
        if (entity->GetParent() != nullptr)
        {
            parent = { { "id", entity->GetParent()->GetId().ToString() } };
        }
        return { { "id", entity->GetId().ToString() }, { "name", entity->GetName() },
            { "active", entity->GetActive() }, { "static", entity->GetStatic() },
            { "parent", parent }, { "components", components } };
    }

    json DeleteHierarchy(Pine::Entity* root, std::unordered_map<std::string, Pine::Entity*>& entitiesByReference,
        json& references)
    {
        std::vector<Pine::Entity*> hierarchy{ root };
        json removed = json::array();
        for (std::size_t index = 0; index < hierarchy.size(); index++)
        {
            const auto entity = hierarchy[index];
            hierarchy.insert(hierarchy.end(), entity->GetChildren().begin(), entity->GetChildren().end());

            // Capture persistent IDs before destruction or reuse of any pool slot.
            json componentIds = json::array();
            for (const auto component : entity->GetComponents())
            {
                componentIds.push_back(component->GetId().ToString());
            }
            removed.push_back({ { "id", entity->GetId().ToString() }, { "componentIds", componentIds } });
        }

        const std::unordered_set<Pine::Entity*> deleted(hierarchy.begin(), hierarchy.end());
        for (auto entry = entitiesByReference.begin(); entry != entitiesByReference.end();)
        {
            if (deleted.count(entry->second) != 0)
            {
                references[entry->first] = nullptr;
                entry = entitiesByReference.erase(entry);
            }
            else
            {
                ++entry;
            }
        }

        for (const auto entity : hierarchy)
        {
            Panels::EntityList::CancelEntityDrag(entity);
            if (Selection::IsSelected(entity))
            {
                // AddEntity toggles an existing selection off without disturbing survivors.
                Selection::AddEntity(entity);
            }
        }

        const auto clearDeletedCamera = [&](Pine::RenderingContext* context)
        {
            if (context != nullptr && context->SceneCamera != nullptr
                && deleted.count(context->SceneCamera->GetParent()) != 0)
            {
                context->SceneCamera = nullptr;
            }
        };
        for (const auto context : Pine::RenderManager::GetRenderingContexts())
        {
            clearDeletedCamera(context);
        }
        clearDeletedCamera(Pine::RenderManager::GetDefaultRenderingContext());

        if (!Pine::Entities::Delete(root))
        {
            throw std::runtime_error("Could not delete entity hierarchy.");
        }
        return removed;
    }

    Response Execute(const std::vector<Operation>& operations)
    {
        json body = { { "phase", "execution" }, { "completed", 0 },
            { "refs", json::object() }, { "results", json::array() } };
        std::unordered_map<std::string, Pine::Entity*> entitiesByReference;
        // Includes unnamed creations and descendants of earlier duplicates. These keys are
        // internal to preparation/execution; only the caller's explicit refs are public.
        std::unordered_map<std::string, Pine::Entity*> entitiesByKey;
        for (const auto entity : Pine::Entities::GetList())
        {
            entitiesByKey[entity->GetId().ToString()] = entity;
        }

        for (std::size_t index = 0; index < operations.size(); index++)
        {
            const auto& operation = operations[index];
            Pine::Entity* created = nullptr;
            Pine::Component* added = nullptr;
            json createdEntities = json::array();
            try
            {
                if (operation.Type == OperationType::CreateEntity)
                {
                    created = Pine::Entity::Create(operation.Name);
                    entitiesByKey[CreationKey(operation, index)] = created;
                    if (!operation.Reference.empty())
                    {
                        entitiesByReference[operation.Reference] = created;
                        body["refs"][operation.Reference] = created->GetId().ToString();
                    }

                    auto parent = operation.Parent;
                    if (!operation.ParentReference.empty())
                    {
                        parent = entitiesByReference.at(operation.ParentReference);
                    }
                    if (parent != nullptr)
                    {
                        parent->AddChild(created);
                    }

                    for (const auto& component : operation.Components)
                    {
                        const auto target = component.Adapter->Type == Pine::ComponentType::Transform
                            ? created->GetTransform() : created->AddComponent(component.Adapter->Type);
                        component.Adapter->Apply(target, component.State);
                    }
                    body["results"].push_back({ { "operation", index }, { "entity", DescribeEntity(created) } });
                }
                else if (operation.Type == OperationType::DuplicateEntity)
                {
                    std::vector<Pine::Entity*> copies;
                    for (const auto& node : operation.DuplicatedEntities)
                    {
                        const auto source = entitiesByKey.at(node.SourceKey);
                        const auto copy = Pine::Entity::Create();
                        const auto sourceTransform = source->GetComponents().front()->GetId().ToString();
                        createdEntities.push_back({ { "sourceId", source->GetId().ToString() },
                            { "id", copy->GetId().ToString() }, { "componentIds", {
                                { sourceTransform, copy->GetTransform()->GetId().ToString() }
                            } } });
                        if (created == nullptr)
                        {
                            created = copy;
                            if (!operation.Reference.empty())
                            {
                                entitiesByReference[operation.Reference] = copy;
                                body["refs"][operation.Reference] = copy->GetId().ToString();
                            }
                        }
                        entitiesByKey[node.Key] = copy;
                        copies.push_back(copy);
                        if (!node.ParentKey.empty())
                        {
                            entitiesByKey.at(node.ParentKey)->AddChild(copy);
                        }

                        Duplication::ApplyEntity(copy, node.State);
                        for (std::size_t componentIndex = 0; componentIndex < node.State.Components.size(); componentIndex++)
                        {
                            const auto& state = node.State.Components[componentIndex];
                            const auto component = state.Type == Pine::ComponentType::Transform
                                ? copy->GetTransform() : copy->AddComponent(state.Type);
                            const auto sourceId = source->GetComponents().at(componentIndex)->GetId().ToString();
                            createdEntities.back()["componentIds"][sourceId] = component->GetId().ToString();
                            Duplication::ApplyComponent(component, state);
                        }
                    }
                    Adapters::Transform::MarkHierarchyDirty(created);

                    json duplicated = json::array();
                    for (std::size_t node = 0; node < copies.size(); node++)
                    {
                        duplicated.push_back({ { "sourceId", createdEntities[node]["sourceId"] },
                            { "entity", DescribeEntity(copies[node]) },
                            { "componentIds", createdEntities[node]["componentIds"] } });
                    }
                    body["results"].push_back({ { "operation", index }, { "entity", DescribeEntity(created) },
                        { "duplicatedEntities", duplicated } });
                }
                else if (operation.Type == OperationType::UpdateEntity)
                {
                    ApplyEntityUpdate(operation.Entity, operation.EntityPatch);
                    body["results"].push_back({ { "operation", index }, { "entity", DescribeEntity(operation.Entity) } });
                }
                else if (operation.Type == OperationType::ReparentEntity)
                {
                    const auto parent = operation.ParentReference.empty()
                        ? operation.Parent : entitiesByReference.at(operation.ParentReference);
                    ApplyReparent(operation.Entity, parent);
                    body["results"].push_back({ { "operation", index }, { "entity", DescribeEntity(operation.Entity) } });
                }
                else if (operation.Type == OperationType::RemoveComponent)
                {
                    const auto entity = operation.Target->GetParent();
                    // Capture identity before destruction; the pool slot may be reused immediately.
                    const json removed = { { "id", operation.Target->GetId().ToString() },
                        { "type", Adapters::Find(operation.Target->GetType())->Name } };
                    if (!Editing::RemoveSceneComponent(operation.Target))
                    {
                        throw std::runtime_error("Could not remove component.");
                    }
                    body["results"].push_back({ { "operation", index },
                        { "entityId", entity->GetId().ToString() }, { "removedComponent", removed } });
                }
                else if (operation.Type == OperationType::DeleteEntity)
                {
                    const auto entityId = operation.Entity->GetId().ToString();
                    const auto removed = DeleteHierarchy(operation.Entity, entitiesByReference, body["refs"]);
                    body["results"].push_back({ { "operation", index }, { "entityId", entityId },
                        { "removedEntities", removed } });
                }
                else
                {
                    const auto& component = operation.Components.front();
                    const auto target = operation.Type == OperationType::AddComponent
                        ? operation.Entity->AddComponent(component.Adapter->Type) : operation.Target;
                    if (operation.Type == OperationType::AddComponent)
                    {
                        added = target;
                        // A remove/add pair can leave the light count unchanged between frames.
                        // Invalidate cached rendering inputs even when all setters keep defaults.
                        operation.Entity->SetDirty(true);
                    }
                    component.Adapter->Apply(target, component.State);
                    body["results"].push_back({ { "operation", index },
                        { "component", Adapters::Describe(*component.Adapter, target) } });
                    if (operation.Type == OperationType::AddComponent)
                    {
                        body["results"].back()["entityId"] = operation.Entity->GetId().ToString();
                    }
                }
                body["completed"] = index + 1;
            }
            catch (const std::exception& exception)
            {
                // Completed operations remain applied. Even the failing operation may have made
                // progress, so return its known identity rather than implying a rollback occurred.
                body["error"] = exception.what();
                body["operation"] = index;
                body["failedOperationMayHaveChangedState"] = true;
                if (created != nullptr)
                {
                    body["createdEntityId"] = created->GetId().ToString();
                }
                if (operation.Type == OperationType::DuplicateEntity)
                {
                    body["createdEntities"] = createdEntities;
                }
                if (added != nullptr)
                {
                    body["createdComponentId"] = added->GetId().ToString();
                    body["entityId"] = operation.Entity->GetId().ToString();
                }
                return { 500, body };
            }
        }
        return { 200, body };
    }
}

Editor::DebugServer::Response Editor::DebugServer::Editing::GetSchema(const Request& request)
{
    json components = json::object();
    for (const auto adapter : Components::GetAdapters())
    {
        components[adapter->Name] = { { "properties", adapter->Properties }, { "defaults", adapter->Read(nullptr) },
            { "addable", adapter->AllowAddRemove }, { "removable", adapter->AllowAddRemove } };
    }

    return { 200, {
        { "version", 1 }, { "operations", { "entity.create", "entity.update", "entity.reparent", "entity.delete", "entity.duplicate",
            "component.add", "component.update", "component.remove" } },
        { "maxOperations", MaxOperations }, { "maxBodyBytes", MaxBodyBytes },
        { "maxJsonDepth", MaxJsonDepth },
        { "requestSchema", Schema::Request(MaxOperations) },
        { "operationSchemas", Schema::Operations() }, { "referenceRules", Schema::References() },
        { "levelCamera", LevelCamera::Schema() },
        { "readback", {
            { "entityEndpoint", "/entity" }, { "observationEndpoint", "/observe" },
            { "entityProperties", "properties" }, { "componentProperties", "components[].properties" },
            { "unsupportedComponents", nullptr }, { "temporaryHierarchies", nullptr },
            { "assetReferenceForm", "id" }, { "availableDuringPlay", true },
            { "validatesCurrentValues", false }
        } },
        { "batchRules", {
            { "executionOrder", "arrayOrder" }, { "validation", "wholeBatchBeforeMutation" },
            { "stateForValidation", "afterPrecedingOperations" },
            { "executionFailure", "completedOperationsRemainApplied" }
        } },
        { "requiredPlayState", "Stopped" }, { "undo", true }, { "rollback", false },
        { "history", { { "unit", "batch" }, { "status", "/history" }, { "undo", "/history/undo" },
            { "redo", "/history/redo" }, { "partialFailure", "clearHistory" },
            { "restoredIdentities", "persistentEntityAndComponentIds" } } },
        { "entity", { { "properties", EntityProperties }, { "reparent", {
            { "transformPreservation", "local" }, { "target", "existing entity id" },
            { "parent", "existing entity id, earlier creation/duplication ref, or null for scene root" }
        } }, { "delete", {
            { "target", "existing entity id" }, { "descendants", "delete" },
            { "deletedCreationRefs", nullptr }, { "supportedComponents", { "Transform", "ModelRenderer", "Light", "Camera", "Collider", "RigidBody" } }
        } }, { "duplicate", {
            { "target", "existing entity id" }, { "descendants", "copy" },
            { "parent", "source parent at this point in the batch" }, { "transformPreservation", "local" },
            { "identities", "new entity and component IDs; source-to-copy mappings returned" },
            { "ref", "optional unique root ref, usable by subsequent parent fields" },
            { "assetReferences", "shared" }, { "supportedComponents", { "Transform", "ModelRenderer", "Light", "Camera", "Collider", "RigidBody" } },
            { "maxDuplicatedEntitiesPerBatch", MaxDuplicatedEntities }
        } } } }, { "components", components }
    } };
}

nlohmann::json Editor::DebugServer::Editing::ReadEntityProperties(const Pine::Entity* entity)
{
    if (!IsSceneEntity(entity))
    {
        return nullptr;
    }
    return { { "name", entity->GetName() }, { "active", entity->GetActive() }, { "static", entity->GetStatic() } };
}

nlohmann::json Editor::DebugServer::Editing::ReadComponentProperties(const Pine::Component* component)
{
    if (!IsSceneEntity(component->GetParent()))
    {
        return nullptr;
    }
    const auto adapter = Components::Find(component->GetType());
    return adapter == nullptr ? json(nullptr) : adapter->Read(component);
}

nlohmann::json Editor::DebugServer::Editing::DeleteEntityHierarchy(Pine::Entity* entity)
{
    std::unordered_map<std::string, Pine::Entity*> references;
    json result = json::object();
    return DeleteHierarchy(entity, references, result);
}

bool Editor::DebugServer::Editing::RemoveSceneComponent(Pine::Component* component)
{
    // Camera pointers belong to rendering contexts, not the component pool. Clear them
    // before the slot can be reused by a later operation in this batch or restoration.
    const auto clearCamera = [&](Pine::RenderingContext* context)
    {
        if (context != nullptr && context->SceneCamera == component)
        {
            context->SceneCamera = nullptr;
        }
    };
    for (const auto context : Pine::RenderManager::GetRenderingContexts())
    {
        clearCamera(context);
    }
    clearCamera(Pine::RenderManager::GetDefaultRenderingContext());
    return component->GetParent()->RemoveComponent(component);
}

Editor::DebugServer::Response Editor::DebugServer::Editing::Edit(const Request& request)
{
    int operationIndex = -1;
    std::vector<Operation> operations;
    try
    {
        Values::Require(request.Body.size() <= MaxBodyBytes, "", "Request exceeds 256 KiB.");
        // A depth limit also bounds parsing before any engine state is consulted.
        const auto depthLimit = [](int depth, json::parse_event_t, json&)
        {
            Values::Require(depth <= MaxJsonDepth, "", "JSON nesting exceeds 32 levels.");
            return true;
        };
        const auto body = json::parse(request.Body, depthLimit, false);
        Values::Require(!body.is_discarded(), "", "Request body is not valid JSON.");

        if (PlayHandler::GetGameState() != PlayHandler::EditorGameState::Stopped)
        {
            return { 409, { { "error", "Stop play mode before editing." },
                { "phase", "validation" }, { "completed", 0 } } };
        }

        operations = Prepare(body, operationIndex);
        for (std::size_t index = 0; index < operations.size(); index++)
        {
            const auto& operation = operations[index];
            if (operation.Type == OperationType::UpdateComponent || operation.Type == OperationType::RemoveComponent)
            {
                operationIndex = static_cast<int>(index);
                RequireRestorableComponent(operation.Target, "/operations/" + std::to_string(index) + "/target");
            }
        }
    }
    catch (const Values::ValidationError& exception)
    {
        json body = { { "error", exception.what() }, { "path", exception.Path },
            { "phase", "validation" }, { "completed", 0 } };
        if (operationIndex >= 0)
        {
            body["operation"] = operationIndex;
        }
        return { 400, body };
    }

    History::Snapshot before;
    try
    {
        Editor::Actions::FinishHeldCommand();
        before = History::Capture();
    }
    catch (const std::exception& exception)
    {
        return { 400, { { "phase", "validation" }, { "completed", 0 }, { "error", exception.what() } } };
    }

    auto response = Execute(operations);
    if (response.StatusCode != 200)
    {
        Editor::Actions::ClearHistory();
        response.Body["history"] = "cleared";
        return response;
    }
    try
    {
        History::Record(std::move(before), History::Capture());
        response.Body["history"] = "recorded";
    }
    catch (const std::exception& exception)
    {
        Editor::Actions::ClearHistory();
        response.StatusCode = 500;
        response.Body["history"] = "cleared";
        response.Body["error"] = std::string("Edits applied, but history recording failed: ") + exception.what();
        response.Body["failedOperationMayHaveChangedState"] = true;
    }
    return response;
}

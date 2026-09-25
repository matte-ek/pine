#include "History.hpp"

#include <algorithm>
#include <set>

#include "../Editing.hpp"
#include "../../LevelSettings/LevelSettings.hpp"
#include "../Components/Transform/Transform.hpp"
#include "../Values/Values.hpp"
#include "Other/Actions/Actions.hpp"
#include "Other/PlayHandler/PlayHandler.hpp"
#include "Pine/Engine/Engine.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/World.hpp"

namespace
{
    using namespace Editor::DebugServer;
    namespace History = Editing::History;
    namespace Duplication = Editing::Duplication;
    namespace Adapters = Editing::Components;
    using nlohmann::json;

    bool SameComponent(const Duplication::ComponentState& left, const Duplication::ComponentState& right)
    {
        return left.Type == right.Type && left.SourceId == right.SourceId && left.Properties == right.Properties
            && left.Active == right.Active && left.OverrideStencilBuffer == right.OverrideStencilBuffer
            && left.StencilBufferValue == right.StencilBufferValue
            && left.CameraClearColor == right.CameraClearColor
            && left.CameraOverrideAspectRatio == right.CameraOverrideAspectRatio
            && left.CameraOrthographicSize == right.CameraOrthographicSize;
    }

    bool SameEntity(const History::EntityState& left, const History::EntityState& right)
    {
        const auto& a = left.State;
        const auto& b = right.State;

        return left.Parent == right.Parent && left.Children == right.Children
            && a.Name == b.Name && a.Active == b.Active && a.Static == b.Static && a.Tags == b.Tags
            && a.Components.size() == b.Components.size()
            && std::equal(a.Components.begin(), a.Components.end(), b.Components.begin(), SameComponent);
    }

    const Duplication::ComponentState* FindComponent(const History::EntityState& entity, const Pine::UId id)
    {
        for (const auto& component : entity.State.Components)
        {
            if (component.SourceId == id)
            {
                return &component;
            }
        }

        return nullptr;
    }

    const History::EntityState* FindEntity(const History::Snapshot& snapshot, const std::string& id)
    {
        const auto found = snapshot.Entities.find(id);
        return found == snapshot.Entities.end() ? nullptr : &found->second;
    }

    void Require(const bool condition, const std::string& message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    void Restore(const History::Snapshot& expected, const History::Snapshot& desired)
    {
        const auto current = History::Capture();
        Require(current.Order == expected.Order, "Scene membership/order changed outside history.");

        if (desired.RestoreSettings)
        {
            Require(current.Settings == expected.Settings, "Level settings changed outside history.");
            Require(Pine::World::GetActiveLevel() != nullptr, "No level is active.");
        }

        if (desired.RestoreGameCamera)
        {
            Require(current.GameCamera == expected.GameCamera, "Game camera changed outside history.");
            Require(Pine::RenderManager::GetPrimaryRenderingContext() != nullptr, "Game rendering context is missing.");

            if (desired.GameCamera.IsValid())
            {
                bool cameraWillExist = false;

                for (const auto& [id, entity] : desired.Entities)
                {
                    const auto component = FindComponent(entity, desired.GameCamera);
                    cameraWillExist |= component != nullptr && component->Type == Pine::ComponentType::Camera;
                }

                const auto camera = Pine::Components::FindById(Pine::ComponentType::Camera, desired.GameCamera);
                cameraWillExist |= camera != nullptr
                    && expected.Entities.count(camera->GetParent()->GetId().ToString()) == 0;

                Require(cameraWillExist, "Game camera to restore no longer exists.");
            }
        }

        // Check every affected entity before mutating. Unrelated component values are left alone.
        std::set<std::string> affected;

        for (const auto& [id, entity] : expected.Entities)
        {
            const auto actual = FindEntity(current, id);
            Require(actual != nullptr && SameEntity(*actual, entity), "Entity changed outside history: " + id);
            affected.insert(id);
        }

        for (const auto& [id, entity] : desired.Entities)
        {
            if (FindEntity(expected, id) == nullptr)
            {
                Require(Pine::Entities::Find(Pine::UId(id)) == nullptr, "Restored entity ID is already in use: " + id);
            }
            affected.insert(id);
        }

        std::map<Pine::ComponentType, std::int64_t> counts;
        const auto maximum = Pine::Engine::GetEngineConfiguration().m_MaxObjectCount;
        Require(desired.Order.size() <= maximum, "Restoration exceeds entity capacity.");

        for (const auto adapter : Adapters::GetAdapters())
        {
            counts[adapter->Type] = Pine::Components::GetFreeSlotCount(adapter->Type);
        }

        for (const auto& id : affected)
        {
            const auto previous = FindEntity(expected, id);
            const auto target = FindEntity(desired, id);

            if (previous != nullptr)
            {
                for (const auto& component : previous->State.Components)
                {
                    if (target == nullptr || FindComponent(*target, component.SourceId) == nullptr)
                    {
                        counts[component.Type]++;
                    }
                }
            }

            if (target == nullptr)
            {
                continue;
            }

            for (const auto& component : target->State.Components)
            {
                const auto old = previous == nullptr ? nullptr : FindComponent(*previous, component.SourceId);

                if (old != nullptr && SameComponent(*old, component))
                {
                    continue;
                }

                const auto adapter = Adapters::Find(component.Type);
                Require(adapter != nullptr, "No restoration adapter for component.");
                Adapters::Prepare(*adapter, adapter->Read(nullptr), component.Properties, "/history/properties");

                if (old == nullptr)
                {
                    counts[component.Type]--;
                }
            }
        }

        for (const auto& [type, available] : counts)
        {
            Require(available >= 0, "Restoration exceeds component capacity.");
        }

        // Detach affected children first so deleting a former parent cannot delete a survivor.
        for (const auto& id : affected)
        {
            const auto entity = Pine::Entities::Find(Pine::UId(id));
            const auto previous = FindEntity(expected, id);
            const auto target = FindEntity(desired, id);

            if (entity != nullptr && entity->GetParent() != nullptr
                && (target == nullptr || previous->Parent != target->Parent))
            {
                entity->GetParent()->RemoveChild(entity);
            }
        }

        for (const auto& [id, previous] : expected.Entities)
        {
            auto entity = Pine::Entities::Find(Pine::UId(id));
            const auto target = FindEntity(desired, id);

            if (target == nullptr)
            {
                Editing::DeleteEntityHierarchy(entity);
                continue;
            }

            const auto components = entity->GetComponents();

            for (const auto component : components)
            {
                if (FindComponent(*target, component->GetId()) == nullptr)
                {
                    Require(Editing::RemoveSceneComponent(component), "Could not remove component during restoration.");
                    entity->SetDirty(true);
                }
            }
        }

        // Allocate identities before connecting parents; the saved map is not hierarchy ordered.
        for (const auto& [id, target] : desired.Entities)
        {
            auto entity = Pine::Entities::Find(Pine::UId(id));

            if (entity == nullptr)
            {
                entity = Pine::Entities::CreateWithId(Pine::UId(id));
                entity->GetTransform()->SetId(target.State.Components.front().SourceId);
            }

            Duplication::ApplyEntity(entity, target.State);
            const auto previous = FindEntity(expected, id);

            for (std::size_t index = 0; index < target.State.Components.size(); index++)
            {
                const auto& saved = target.State.Components[index];
                auto component = Pine::Components::FindById(saved.Type, saved.SourceId);

                if (component == nullptr)
                {
                    component = entity->AddComponent(saved.Type);
                    component->SetId(saved.SourceId);
                }

                const auto old = previous == nullptr ? nullptr : FindComponent(*previous, saved.SourceId);

                if (old == nullptr || !SameComponent(*old, saved))
                {
                    Duplication::ApplyComponent(component, saved);
                }

                entity->MoveComponent(component, index);
            }
        }

        for (const auto& [id, target] : desired.Entities)
        {
            const auto entity = Pine::Entities::Find(Pine::UId(id));
            const auto children = entity->GetChildren();

            for (const auto child : children)
            {
                entity->RemoveChild(child);
            }

            for (const auto child : target.Children)
            {
                entity->AddChild(Pine::Entities::Find(child));
            }
        }

        // An affected entity may have an unchanged parent (e.g. a property-only edit).
        for (const auto& [id, target] : desired.Entities)
        {
            const auto entity = Pine::Entities::Find(Pine::UId(id));

            if (target.Parent.IsValid() && entity->GetParent() == nullptr)
            {
                Pine::Entities::Find(target.Parent)->AddChild(entity);
            }

            Adapters::Transform::MarkHierarchyDirty(entity);
        }

        for (std::size_t index = 0; index < desired.Order.size(); index++)
        {
            Pine::Entities::MoveEntity(Pine::Entities::Find(desired.Order[index]), index);
        }

        if (desired.RestoreGameCamera)
        {
            const auto camera = desired.GameCamera.IsValid()
                ? Pine::Components::FindById(Pine::ComponentType::Camera, desired.GameCamera) : nullptr;
            Pine::RenderManager::GetPrimaryRenderingContext()->SceneCamera = static_cast<Pine::Camera*>(camera);
        }

        if (desired.RestoreSettings)
        {
            LevelSettings::Apply(Pine::World::GetActiveLevel()->GetLevelSettings(), desired.Settings);
        }
    }

    class BatchCommand final : public Editor::Actions::EditorCommand
    {
        History::Snapshot m_Before;
        History::Snapshot m_After;
    public:
        BatchCommand(History::Snapshot before, History::Snapshot after)
            : m_Before(std::move(before)), m_After(std::move(after))
        {
        }

        void Apply(const Editor::Actions::CommandState state) override
        {
            if (state == Editor::Actions::CommandState::PreCommand)
            {
                Restore(m_After, m_Before);
            }
            else
            {
                Restore(m_Before, m_After);
            }
        }
    };

    Response Apply(const Request& request, const bool redo)
    {
        const auto body = request.Body.empty() ? json::object() : json::parse(request.Body, nullptr, false);
        if (!body.is_object() || !body.empty())
        {
            return Error(400, "Expected an empty object.");
        }
        if (PlayHandler::GetGameState() != PlayHandler::EditorGameState::Stopped)
        {
            return Error(409, "Stop play mode before undo or redo.");
        }

        const auto result = redo ? Editor::Actions::ExecuteRedo() : Editor::Actions::ExecuteUndo();
        auto response = History::Get(request);
        response.Body["applied"] = result.Applied;

        if (!result.Error.empty())
        {
            response.StatusCode = 500;
            response.Body["error"] = result.Error;
            response.Body["historyCleared"] = true;
            response.Body["stateMayHaveChanged"] = true;
        }

        return response;
    }
}

Editor::DebugServer::Editing::History::Snapshot Editor::DebugServer::Editing::History::Capture()
{
    Snapshot snapshot;

    const auto context = Pine::RenderManager::GetPrimaryRenderingContext();
    if (context != nullptr && context->SceneCamera != nullptr)
    {
        snapshot.GameCamera = context->SceneCamera->GetId();
    }

    if (const auto level = Pine::World::GetActiveLevel())
    {
        snapshot.Settings = LevelSettings::Read(level->GetLevelSettings());
    }

    for (const auto entity : Pine::Entities::GetList())
    {
        snapshot.Order.push_back(entity->GetId());

        if (ReadEntityProperties(entity).is_null())
        {
            continue;
        }

        EntityState saved;
        saved.State = Duplication::Read(entity);

        if (entity->GetParent() != nullptr)
        {
            saved.Parent = entity->GetParent()->GetId();
        }

        for (const auto child : entity->GetChildren())
        {
            saved.Children.push_back(child->GetId());
        }

        snapshot.Entities.emplace(entity->GetId().ToString(), std::move(saved));
    }

    return snapshot;
}

void Editor::DebugServer::Editing::History::Record(Snapshot before, Snapshot after)
{
    before.RestoreGameCamera = after.RestoreGameCamera = before.GameCamera != after.GameCamera;
    before.RestoreSettings = after.RestoreSettings = before.Settings != after.Settings;

    // Retain only affected entities, so later undo does not rewrite unrelated component values.
    for (auto entry = before.Entities.begin(); entry != before.Entities.end();)
    {
        const auto found = after.Entities.find(entry->first);

        if (found != after.Entities.end() && SameEntity(entry->second, found->second))
        {
            after.Entities.erase(found);
            entry = before.Entities.erase(entry);
        }
        else
        {
            ++entry;
        }
    }

    Editor::Actions::RegisterCommand(std::make_unique<BatchCommand>(std::move(before), std::move(after)));
}

Editor::DebugServer::Response Editor::DebugServer::Editing::History::Get(const Request&)
{
    const auto state = Editor::Actions::GetHistoryState();
    return { 200, { { "undoCount", state.UndoCount }, { "redoCount", state.RedoCount },
        { "sceneGeneration", Pine::Entities::GetSceneGeneration() }, { "limit", 128 } } };
}

Editor::DebugServer::Response Editor::DebugServer::Editing::History::Undo(const Request& request)
{
    return Apply(request, false);
}

Editor::DebugServer::Response Editor::DebugServer::Editing::History::Redo(const Request& request)
{
    return Apply(request, true);
}

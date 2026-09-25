#include "Interfaces.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Engine/Engine.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Components/Component/Component.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/Script/Bindings/Bindings.hpp"
#include "Pine/Script/Factory/ScriptObjectFactory.hpp"
#include "Pine/World/Components/Script/ScriptComponent.hpp"

namespace
{
    // The managed mirror of a component, or of the user's script object for a Script component -
    // which is what C# expects back from GetComponent<Script>() and GetScript<T>().
    Pine::Script::ObjectHandle* ComponentHandleOf(Pine::Component* component)
    {
        if (component->GetType() == Pine::ComponentType::Script)
        {
            return dynamic_cast<Pine::ScriptComponent*>(component)->GetScriptObjectHandle();
        }

        return component->GetComponentScriptHandle();
    }

    const char* GetEntityName(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return nullptr;
        return Pine::Script::Bindings::ReturnString(Pine::Entities::GetByInternalId(internalId)->GetName());
    }

    void SetEntityName(const std::uint32_t internalId, const char* name)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;
        Pine::Entities::GetByInternalId(internalId)->SetName(name);
    }

    bool GetEntityActive(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return false;
        return Pine::Entities::GetByInternalId(internalId)->GetActive();
    }

    void SetEntityActive(const std::uint32_t internalId, const bool active)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;
        Pine::Entities::GetByInternalId(internalId)->SetActive(active);
    }

    bool GetEntityStatic(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return false;
        return Pine::Entities::GetByInternalId(internalId)->GetStatic();
    }

    void SetEntityStatic(const std::uint32_t internalId, const bool active)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;
        Pine::Entities::GetByInternalId(internalId)->SetStatic(active);
    }

    std::uint64_t GetEntityTags(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return false;
        return Pine::Entities::GetByInternalId(internalId)->GetTags();
    }

    void SetEntityTags(const std::uint32_t internalId, const std::uint64_t tags)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;
        Pine::Entities::GetByInternalId(internalId)->SetTags(tags);
    }

    void DestroyEntity(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Entities::Delete(Pine::Entities::GetByInternalId(internalId));
    }

    // C# builds the array itself, so everything that hands back a set of objects is a count and
    // an indexed read of one handle. The engine cannot allocate a managed array.
    int GetChildCount(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0;

        auto entity = Pine::Entities::GetByInternalId(internalId);

        if (!entity)
        {
            return 0;
        }

        return static_cast<int>(entity->GetChildren().size());
    }

    std::uint64_t GetChild(const std::uint32_t internalId, const int index)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0;

        auto entity = Pine::Entities::GetByInternalId(internalId);

        if (!entity || index < 0 || index >= static_cast<int>(entity->GetChildren().size()))
        {
            return 0;
        }

        return entity->GetChildren()[index]->GetScriptHandle()->Id;
    }

    std::uint64_t GetTransform(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0;

        return Pine::Entities::GetByInternalId(internalId)->GetTransform()->GetComponentScriptHandle()->Id;
    }

    // C# resolves its component classes to a ComponentType itself (see ComponentTypes.Of<T>) and
    // passes the value, so nothing here has to inspect a System.Type. A class that named no
    // component type arrives as something outside the enum, and is treated as no component at all
    // rather than as whichever one happens to sit at that value.
    bool IsComponentType(const int type)
    {
        // Against the registered data blocks rather than a literal bound: the enum and the block
        // list are kept one-to-one by Pine::Components::Setup, so this stays right as components
        // are added.
        return 0 <= type && type < static_cast<int>(Pine::Components::GetComponentTypes().size());
    }

    bool HasComponent(const std::uint32_t id, const int type)
    {
        if (std::numeric_limits<std::uint32_t>::max() == id) return false;
        if (!IsComponentType(type)) return false;

        const auto componentType = static_cast<Pine::ComponentType>(type);

        for (const auto& component : Pine::Entities::GetByInternalId(id)->GetComponents())
        {
            if (component->GetType() == componentType)
            {
                return true;
            }
        }

        return false;
    }

    std::uint64_t GetComponent(const std::uint32_t id, const int type)
    {
        if (std::numeric_limits<std::uint32_t>::max() == id) return 0;
        if (!IsComponentType(type)) return 0;

        const auto componentType = static_cast<Pine::ComponentType>(type);

        for (const auto& component : Pine::Entities::GetByInternalId(id)->GetComponents())
        {
            if (component->GetType() == componentType)
            {
                return ComponentHandleOf(component)->Id;
            }
        }

        return 0;
    }

    // The n-th component of this type, found by counting rather than by building a list: an entity
    // holds a handful of components, so walking them again per index costs nothing worth keeping
    // state for.
    Pine::Component* GetComponentOfType(const std::uint32_t id, const int type, const int index)
    {
        if (std::numeric_limits<std::uint32_t>::max() == id) return nullptr;
        if (!IsComponentType(type) || index < 0) return nullptr;

        const auto componentType = static_cast<Pine::ComponentType>(type);
        auto remaining = index;

        for (const auto& component : Pine::Entities::GetByInternalId(id)->GetComponents())
        {
            if (component->GetType() != componentType)
            {
                continue;
            }

            if (remaining-- == 0)
            {
                return component;
            }
        }

        return nullptr;
    }

    int GetComponentCount(const std::uint32_t id, const int type)
    {
        if (std::numeric_limits<std::uint32_t>::max() == id) return 0;
        if (!IsComponentType(type)) return 0;

        const auto componentType = static_cast<Pine::ComponentType>(type);
        auto count = 0;

        for (const auto& component : Pine::Entities::GetByInternalId(id)->GetComponents())
        {
            if (component->GetType() == componentType)
            {
                count++;
            }
        }

        return count;
    }

    std::uint64_t GetComponentAt(const std::uint32_t id, const int type, const int index)
    {
        const auto component = GetComponentOfType(id, type, index);

        return component ? ComponentHandleOf(component)->Id : 0;
    }

    // Creating past the end of a pool throws, and that exception must not unwind into the managed
    // code that called the binding: it would take the process down. So the bindings that create
    // check for room first, and C# gets null instead.
    bool HasRoomForEntity()
    {
        if (Pine::Entities::GetFreeSlotCount() > 0)
        {
            return true;
        }

        PError(fmt::format("Cannot create an entity, all {} entity slots are in use.",
            Pine::Engine::GetEngineConfiguration().m_MaxObjectCount));

        return false;
    }

    bool HasRoomForComponent(const Pine::ComponentType type)
    {
        if (Pine::Components::GetFreeSlotCount(type) > 0)
        {
            return true;
        }

        PError(fmt::format("Cannot create a {} component, all {} of its slots are in use.",
            Pine::ComponentTypeToString(type), Pine::Components::GetData(type).m_ComponentArrayAllocatedCount));

        return false;
    }

    std::uint64_t AddComponent(const std::uint32_t id, const int type)
    {
        if (std::numeric_limits<std::uint32_t>::max() == id) return 0;
        if (!IsComponentType(type)) return 0;

        const auto componentType = static_cast<Pine::ComponentType>(type);

        if (!HasRoomForComponent(componentType)) return 0;

        const auto component = Pine::Entities::GetByInternalId(id)->AddComponent(componentType);

        return component->GetComponentScriptHandle()->Id;
    }

    std::uint64_t CreateEntity(const char* name)
    {
        // Every entity is created with a Transform.
        if (!HasRoomForEntity() || !HasRoomForComponent(Pine::ComponentType::Transform)) return 0;

        return Pine::Entities::Create(name)->GetScriptHandle()->Id;
    }

    std::uint64_t FindEntityByName(const char* name)
    {
        const auto entity = Pine::Entities::Find(name);

        if (!entity)
        {
            return 0;
        }

        return entity->GetScriptHandle()->Id;
    }

    int FindEntityByTagCount(const std::uint64_t tag)
    {
        auto count = 0;

        for (const auto& entity : Pine::Entities::GetList())
        {
            if (entity->GetTags() & tag)
            {
                count++;
            }
        }

        return count;
    }

    // Walks to the n-th match rather than keeping the previous query around, so two calls can
    // never disagree about a scene that changed between them. Find(tag) is a lookup, not something
    // a frame does repeatedly, and the scene entity list is the only thing being scanned.
    std::uint64_t FindEntityByTagAt(const std::uint64_t tag, const int index)
    {
        if (index < 0) return 0;

        auto remaining = index;

        for (const auto& entity : Pine::Entities::GetList())
        {
            if (!(entity->GetTags() & tag))
            {
                continue;
            }

            if (remaining-- == 0)
            {
                return entity->GetScriptHandle()->Id;
            }
        }

        return 0;
    }

    int GetEntityCount()
    {
        return static_cast<int>(Pine::Entities::GetList().size());
    }

    std::uint64_t GetEntityAt(const int index)
    {
        const auto& entities = Pine::Entities::GetList();

        if (index < 0 || index >= static_cast<int>(entities.size()))
        {
            return 0;
        }

        return entities[index]->GetScriptHandle()->Id;
    }

}

void Pine::Script::Interfaces::Entity::Setup()
{
    Bindings::Register("Pine.World.Entity::GetName", GetEntityName);
    Bindings::Register("Pine.World.Entity::SetName", SetEntityName);
    Bindings::Register("Pine.World.Entity::GetActive", GetEntityActive);
    Bindings::Register("Pine.World.Entity::SetActive", SetEntityActive);
    Bindings::Register("Pine.World.Entity::GetStatic", GetEntityStatic);
    Bindings::Register("Pine.World.Entity::SetStatic", SetEntityStatic);
    Bindings::Register("Pine.World.Entity::GetTags", GetEntityTags);
    Bindings::Register("Pine.World.Entity::SetTags", SetEntityTags);
    Bindings::Register("Pine.World.Entity::GetChildCount", GetChildCount);
    Bindings::Register("Pine.World.Entity::GetChild", GetChild);
    Bindings::Register("Pine.World.Entity::GetTransform", GetTransform);
    Bindings::Register("Pine.World.Entity::HasComponent", HasComponent);
    Bindings::Register("Pine.World.Entity::GetComponent", GetComponent);
    Bindings::Register("Pine.World.Entity::GetComponentCount", GetComponentCount);
    Bindings::Register("Pine.World.Entity::GetComponentAt", GetComponentAt);
    Bindings::Register("Pine.World.Entity::AddComponent", AddComponent);
    Bindings::Register("Pine.World.Entity::CreateEntity", CreateEntity);
    Bindings::Register("Pine.World.Entity::DestroyEntity", DestroyEntity);

    Bindings::Register("Pine.World.EntityList::FindByName", FindEntityByName);
    Bindings::Register("Pine.World.EntityList::FindByTagCount", FindEntityByTagCount);
    Bindings::Register("Pine.World.EntityList::FindByTagAt", FindEntityByTagAt);
    Bindings::Register("Pine.World.EntityList::GetCount", GetEntityCount);
    Bindings::Register("Pine.World.EntityList::GetAt", GetEntityAt);
}
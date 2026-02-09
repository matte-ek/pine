#include "Interfaces.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Components/Component/Component.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "mono/metadata/class.h"
#include "mono/metadata/metadata.h"
#include "mono/utils/mono-publib.h"
#include <cstring>
#include <mono/metadata/object.h>
#include <mono/metadata/appdomain.h>
#include <mono/metadata/exception.h>
#include <unordered_map>

#include "Pine/Script/Runtime/ScriptingRuntime.hpp"
#include "Pine/World/Components/Script/ScriptComponent.hpp"

namespace
{
    std::unordered_map<MonoType*, Pine::ComponentType> m_ComponentTypeLookupMap;

    MonoString* GetEntityName(std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return nullptr;
        return mono_string_new(mono_domain_get(), Pine::Entities::GetByInternalId(internalId)->GetName().c_str());
    }

    void SetEntityName(std::uint32_t internalId, MonoString* name)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;
        Pine::Entities::GetByInternalId(internalId)->SetName(mono_string_to_utf8(name));
    }

    bool GetEntityActive(std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return false;
        return Pine::Entities::GetByInternalId(internalId)->GetActive();
    }

    void SetEntityActive(std::uint32_t internalId, bool active)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;
        Pine::Entities::GetByInternalId(internalId)->SetActive(active);
    }

    bool GetEntityStatic(std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return false;
        return Pine::Entities::GetByInternalId(internalId)->GetStatic();
    }

    void SetEntityStatic(std::uint32_t internalId, bool active)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;
        Pine::Entities::GetByInternalId(internalId)->SetStatic(active);
    }

    std::uint64_t GetEntityTags(std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return false;
        return Pine::Entities::GetByInternalId(internalId)->GetTags();
    }

    void SetEntityTags(std::uint32_t internalId, std::uint64_t tags)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;
        Pine::Entities::GetByInternalId(internalId)->SetTags(tags);
    }

    void DestroyEntity(std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Entities::Delete(Pine::Entities::GetByInternalId(internalId));
    }

    MonoArray* GetChildren(std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return nullptr;

        auto entity = Pine::Entities::GetByInternalId(internalId);

        if (!entity)
        {
            return nullptr;
        }

        auto& children = entity->GetChildren();
        auto arr = mono_array_new(mono_domain_get(), Pine::Script::ObjectFactory::GetEntityClass(), children.size());

        for (int i = 0; i < children.size();i++)
        {
            mono_array_setref(arr, i, mono_gchandle_get_target(entity->GetChildren()[i]->GetScriptHandle()->Handle));
            //mono_array_set(arr, MonoObject*, i, mono_gchandle_get_target(entity->GetChildren()[i]->GetScriptHandle()->Handle));
        }

        return arr;
    }

    MonoObject* GetTransform(std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return nullptr;

        return mono_gchandle_get_target(Pine::Entities::GetByInternalId(internalId)->GetTransform()->GetComponentScriptHandle()->Handle);
    }

    Pine::ComponentType GetComponentType(MonoReflectionType* reflectionType)
    {
        auto type = mono_reflection_type_get_type(reflectionType);

        if (m_ComponentTypeLookupMap.count(type) == 0)
        {
            auto componentType = Pine::ComponentType::Transform;
            auto name = mono_type_get_name(type) + 22; // We add 22 to ignore the "Pine.World.Components." part of the string.

            for (const auto& componentBlock : Pine::Components::GetComponentTypes())
            {
                auto s = Pine::ComponentTypeToString(componentBlock->m_Component->GetType());
                if (strcmp(s, name) == 0)
                {
                    componentType = componentBlock->m_Component->GetType();
                    break;
                }
            }

            m_ComponentTypeLookupMap[type] = componentType;
        }

        return m_ComponentTypeLookupMap[type];
    }

    bool HasComponent(std::uint32_t id, MonoReflectionType* reflectionType)
    {
        if (std::numeric_limits<std::uint32_t>::max() == id) return false;

        auto componentType = GetComponentType(reflectionType);

        for (const auto& component : Pine::Entities::GetByInternalId(id)->GetComponents())
        {
            if (component->GetType() == componentType)
            {
                return true;
            }
        }

        return false;
    }

    MonoObject* GetComponent(std::uint32_t id, MonoReflectionType* reflectionType)
    {
        if (std::numeric_limits<std::uint32_t>::max() == id) return nullptr;

        auto componentType = GetComponentType(reflectionType);

        for (const auto& component : Pine::Entities::GetByInternalId(id)->GetComponents())
        {
            if (component->GetType() == componentType)
            {
                if (component->GetType() == Pine::ComponentType::Script)
                {
                    return mono_gchandle_get_target(dynamic_cast<Pine::ScriptComponent*>(component)->GetScriptObjectHandle()->Handle);
                }

                return mono_gchandle_get_target(component->GetComponentScriptHandle()->Handle);
            }
        }

        return nullptr;
    }

    MonoArray* GetComponents(std::uint32_t id, MonoReflectionType* reflectionType)
    {
        if (std::numeric_limits<std::uint32_t>::max() == id) return nullptr;

        auto componentType = GetComponentType(reflectionType);

        std::vector<Pine::Component*> components;

        for (const auto& component : Pine::Entities::GetByInternalId(id)->GetComponents())
        {
            if (component->GetType() == componentType)
            {
                components.push_back(component);
            }
        }

        auto arr = mono_array_new(mono_domain_get(), Pine::Script::ObjectFactory::GetComponentClass(componentType), components.size());
        for (int i = 0; i < components.size();i++)
        {
            uint32_t handle = components[i]->GetComponentScriptHandle()->Handle;

            if (components[i]->GetType() == Pine::ComponentType::Script)
            {
                handle = dynamic_cast<Pine::ScriptComponent*>(components[i])->GetScriptObjectHandle()->Handle;
            }

            mono_array_setref(arr, i, mono_gchandle_get_target(handle));
        }

        return arr;
    }

    MonoObject* AddComponent(std::uint32_t id, MonoReflectionType* reflectionType)
    {
        if (std::numeric_limits<std::uint32_t>::max() == id) return nullptr;

        auto componentType = GetComponentType(reflectionType);

        return mono_gchandle_get_target(Pine::Entities::GetByInternalId(id)->AddComponent(componentType)->GetComponentScriptHandle()->Handle);
    }

    MonoObject* CreateEntity(MonoString* name)
    {
        return mono_gchandle_get_target(Pine::Entities::Create(mono_string_to_utf8(name))->GetScriptHandle()->Handle);
    }

    MonoObject* FindEntityByName(MonoString* name)
    {
        auto entity = Pine::Entities::Find(mono_string_to_utf8(name));

        if (!entity)
        {
            return nullptr;
        }

        return mono_gchandle_get_target(entity->GetScriptHandle()->Handle);
    }

    MonoArray* FindEntityByTag(uint64_t tag)
    {
        std::vector<Pine::Entity*> entities;

        for (const auto& entity : Pine::Entities::GetList())
        {
            if (entity->GetTags() & tag)
            {
                entities.push_back(entity);
            }
        }

        auto arr = mono_array_new(mono_domain_get(), Pine::Script::ObjectFactory::GetEntityClass(), entities.size());

        for (int i = 0; i < entities.size();i++)
        {
            mono_array_setref(arr, i, mono_gchandle_get_target(entities[i]->GetScriptHandle()->Handle));
        }

        return arr;
    }

    MonoArray* GetAll()
    {
        auto arr = mono_array_new(mono_domain_get(), Pine::Script::ObjectFactory::GetEntityClass(), Pine::Entities::GetList().size());

        for (int i = 0; i < Pine::Entities::GetList().size();i++)
        {
            mono_array_setref(arr, i, mono_gchandle_get_target(Pine::Entities::GetList()[i]->GetScriptHandle()->Handle));
        }

        return arr;
    }

}

void Pine::Script::Interfaces::Entity::Setup()
{
    m_ComponentTypeLookupMap.clear();

    mono_add_internal_call("Pine.World.Entity::GetName", reinterpret_cast<void*>(GetEntityName));
    mono_add_internal_call("Pine.World.Entity::SetName", reinterpret_cast<void*>(SetEntityName));
    mono_add_internal_call("Pine.World.Entity::GetActive", reinterpret_cast<void*>(GetEntityActive));
    mono_add_internal_call("Pine.World.Entity::SetActive", reinterpret_cast<void*>(SetEntityActive));
    mono_add_internal_call("Pine.World.Entity::GetStatic", reinterpret_cast<void*>(GetEntityStatic));
    mono_add_internal_call("Pine.World.Entity::SetStatic", reinterpret_cast<void*>(SetEntityStatic));
    mono_add_internal_call("Pine.World.Entity::GetTags", reinterpret_cast<void*>(GetEntityTags));
    mono_add_internal_call("Pine.World.Entity::SetTags", reinterpret_cast<void*>(SetEntityTags));
    mono_add_internal_call("Pine.World.Entity::GetChildren", reinterpret_cast<void*>(GetChildren));
    mono_add_internal_call("Pine.World.Entity::GetTransform", reinterpret_cast<void*>(GetTransform));
    mono_add_internal_call("Pine.World.Entity::HasComponent", reinterpret_cast<void*>(HasComponent));
    mono_add_internal_call("Pine.World.Entity::GetComponent", reinterpret_cast<void*>(GetComponent));
    mono_add_internal_call("Pine.World.Entity::GetComponents", reinterpret_cast<void*>(GetComponents));
    mono_add_internal_call("Pine.World.Entity::AddComponent", reinterpret_cast<void*>(AddComponent));
    mono_add_internal_call("Pine.World.Entity::CreateEntity", reinterpret_cast<void*>(CreateEntity));
    mono_add_internal_call("Pine.World.Entity::DestroyEntity", reinterpret_cast<void*>(DestroyEntity));

    mono_add_internal_call("Pine.World.EntityList::FindByName", reinterpret_cast<void*>(FindEntityByName));
    mono_add_internal_call("Pine.World.EntityList::FindByTag", reinterpret_cast<void*>(FindEntityByTag));
    mono_add_internal_call("Pine.World.EntityList::GetAll", reinterpret_cast<void*>(GetAll));

}
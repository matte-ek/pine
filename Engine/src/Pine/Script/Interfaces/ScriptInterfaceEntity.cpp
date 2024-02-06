#include "Interfaces.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include <mono/metadata/appdomain.h>

namespace
{
    MonoString* GetEntityName(int internalId)
    {
        return mono_string_new(mono_domain_get(), Pine::Entities::GetByInternalId(internalId)->GetName().c_str());
    }

    void SetEntityName(int internalId, MonoString* name)
    {
        Pine::Entities::GetByInternalId(internalId)->SetName(mono_string_to_utf8(name));
    }

    MonoObject* GetTransform(int internalId)
    {
        return mono_gchandle_get_target(Pine::Entities::GetByInternalId(internalId)->GetTransform()->GetComponentScriptHandle()->Handle);
    }
}

void Pine::Script::Interfaces::Entity::Setup()
{
    mono_add_internal_call("Pine.World.Entity::GetName", reinterpret_cast<void*>(GetEntityName));
    mono_add_internal_call("Pine.World.Entity::SetName", reinterpret_cast<void*>(SetEntityName));
    mono_add_internal_call("Pine.World.Entity::GetTransform", reinterpret_cast<void*>(GetTransform));
}
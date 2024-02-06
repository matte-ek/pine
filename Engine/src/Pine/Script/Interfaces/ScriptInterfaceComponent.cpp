#include "Interfaces.hpp"
#include "Pine/World/Components/IComponent/IComponent.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Components/Components.hpp"
#include <mono/metadata/appdomain.h>

namespace
{
    bool GetActive(int internalId, int type)
    {
        return Pine::Components::GetByInternalId(static_cast<Pine::ComponentType>(type), internalId)->GetActive();
    }

    void SetPosition(int internalId, int type, bool active)
    {
        Pine::Components::GetByInternalId(static_cast<Pine::ComponentType>(type), internalId)->SetActive(active);
    }
}

void Pine::Script::Interfaces::Component::Setup()
{
    mono_add_internal_call("Pine.World.Component::GetActive", reinterpret_cast<void *>(GetActive));
    mono_add_internal_call("Pine.World.Component::SetActive", reinterpret_cast<void *>(SetPosition));
}
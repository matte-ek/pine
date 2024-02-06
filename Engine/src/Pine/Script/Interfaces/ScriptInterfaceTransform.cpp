#include "Interfaces.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Components/Transform/Transform.hpp"
#include <mono/metadata/appdomain.h>

namespace
{
    void GetPosition(int internalId, Pine::Vector3f *position)
    {
        *position = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->LocalPosition;
    }

    void SetPosition(int internalId, Pine::Vector3f *position)
    {
        Pine::Components::GetByInternalId<Pine::Transform>(internalId)->LocalPosition = *position;
    }

    void GetRotation(int internalId, Pine::Quaternion *rotation)
    {
        *rotation = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->LocalRotation;
    }

    void SetRotation(int internalId, Pine::Quaternion *rotation)
    {
        Pine::Components::GetByInternalId<Pine::Transform>(internalId)->LocalRotation = *rotation;
    }

    void GetScale(int internalId, Pine::Vector3f *scale)
    {
        *scale = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->LocalScale;
    }

    void SetScale(int internalId, Pine::Vector3f *scale)
    {
        Pine::Components::GetByInternalId<Pine::Transform>(internalId)->LocalScale = *scale;
    }

    void GetUp(int internalId, Pine::Vector3f *up)
    {
        *up = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->GetUp();
    }

    void GetRight(int internalId, Pine::Vector3f *right)
    {
        *right = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->GetRight();
    }

    void GetForward(int internalId, Pine::Vector3f *forward)
    {
        *forward = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->GetForward();
    }
}

void Pine::Script::Interfaces::Transform::Setup()
{
    mono_add_internal_call("Pine.World.Components.Transform::GetLocalPosition", reinterpret_cast<void *>(GetPosition));
    mono_add_internal_call("Pine.World.Components.Transform::SetLocalPosition", reinterpret_cast<void *>(SetPosition));
    mono_add_internal_call("Pine.World.Components.Transform::GetLocalRotation", reinterpret_cast<void *>(GetRotation));
    mono_add_internal_call("Pine.World.Components.Transform::SetLocalRotation", reinterpret_cast<void *>(SetRotation));
    mono_add_internal_call("Pine.World.Components.Transform::GetLocalScale", reinterpret_cast<void *>(GetScale));
    mono_add_internal_call("Pine.World.Components.Transform::SetLocalScale", reinterpret_cast<void *>(SetScale));
    mono_add_internal_call("Pine.World.Components.Transform::GetUp", reinterpret_cast<void *>(GetUp));
    mono_add_internal_call("Pine.World.Components.Transform::GetRight", reinterpret_cast<void *>(GetRight));
    mono_add_internal_call("Pine.World.Components.Transform::GetForward", reinterpret_cast<void *>(GetForward));
}
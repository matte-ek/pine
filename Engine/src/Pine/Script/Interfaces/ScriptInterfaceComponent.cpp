#include "Interfaces.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/World/Components/IComponent/IComponent.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Components/Components.hpp"
#include "mono/metadata/object.h"
#include "Pine/World/Components/RigidBody/RigidBody.hpp"

namespace
{
    bool GetActive(std::uint32_t internalId, int type)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return false;
        return Pine::Components::GetByInternalId(static_cast<Pine::ComponentType>(type), internalId)->GetActive();
    }

    void SetActive(std::uint32_t internalId, int type, bool active)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;
        Pine::Components::GetByInternalId(static_cast<Pine::ComponentType>(type), internalId)->SetActive(active);
    }

    // -----------------------------------------------------

    void SetModel(std::uint32_t internalId, std::uint32_t assetId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        dynamic_cast<Pine::ModelRenderer*>(Pine::Components::GetByInternalId(Pine::ComponentType::ModelRenderer, internalId))->SetModel(
            dynamic_cast<Pine::Model*>(Pine::Assets::GetById(assetId))
        );
    }

    MonoObject* GetModel(std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return nullptr;

        return mono_gchandle_get_target(dynamic_cast<Pine::ModelRenderer*>(Pine::Components::GetByInternalId(Pine::ComponentType::ModelRenderer, internalId))->GetModel()->GetScriptHandle()->Handle);
    }

    // -----------------------------------------------------

    void TransformGetPosition(std::uint32_t internalId, Pine::Vector3f *position)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        *position = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->GetPosition();
    }

    void TransformGetRotation(std::uint32_t internalId, Pine::Quaternion *rotation)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        *rotation = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->GetRotation();
    }

    void TransformGetScale(std::uint32_t internalId, Pine::Vector3f *scale)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        *scale = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->GetScale();
    }

    void TransformGetLocalPosition(std::uint32_t internalId, Pine::Vector3f *position)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        *position = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->GetLocalPosition();
    }

    void TransformSetLocalPosition(std::uint32_t internalId, Pine::Vector3f *position)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Transform>(internalId)->SetLocalPosition(*position);
    }

    void TransformGetLocalRotation(std::uint32_t internalId, Pine::Quaternion *rotation)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        *rotation = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->GetLocalRotation();
    }

    void TransformSetLocalRotation(std::uint32_t internalId, Pine::Quaternion *rotation)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Transform>(internalId)->SetLocalRotation(*rotation);
    }

    void TransformGetLocalEulerAngles(std::uint32_t internalId, Pine::Vector3f *rotation)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        *rotation = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->GetEulerAngles();
    }

    void TransformSetLocalEulerAngles(std::uint32_t internalId, Pine::Vector3f *rotation)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Transform>(internalId)->SetEulerAngles(*rotation);
    }

    void TransformGetLocalScale(std::uint32_t internalId, Pine::Vector3f *scale)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        *scale = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->GetLocalScale();
    }

    void TransformSetLocalScale(std::uint32_t internalId, Pine::Vector3f *scale)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Transform>(internalId)->SetLocalScale(*scale);
    }

    void TransformGetUp(std::uint32_t internalId, Pine::Vector3f *up)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        *up = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->GetUp();
    }

    void TransformGetRight(std::uint32_t internalId, Pine::Vector3f *right)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        *right = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->GetRight();
    }

    void TransformGetForward(std::uint32_t internalId, Pine::Vector3f *forward)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        *forward = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->GetForward();
    }

    // -----------------------------------------------------

    void RigidBodyApplyForce(std::uint32_t internalId, const Pine::Vector3f *force, physx::PxForceMode::Enum mode)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::RigidBody>(internalId)->ApplyForce(*force, mode);
    }
}

void Pine::Script::Interfaces::Component::Setup()
{
    mono_add_internal_call("Pine.World.Component::GetActive", reinterpret_cast<void *>(GetActive));
    mono_add_internal_call("Pine.World.Component::SetActive", reinterpret_cast<void *>(SetActive));

    mono_add_internal_call("Pine.World.Components.ModelRenderer::SetModel", reinterpret_cast<void *>(SetModel));
    mono_add_internal_call("Pine.World.Components.ModelRenderer::GetModel", reinterpret_cast<void *>(GetModel));

    mono_add_internal_call("Pine.World.Components.RigidBody::ApplyForce", reinterpret_cast<void *>(RigidBodyApplyForce));

    mono_add_internal_call("Pine.World.Components.Transform::GetPosition", reinterpret_cast<void *>(TransformGetPosition));
    mono_add_internal_call("Pine.World.Components.Transform::GetRotation", reinterpret_cast<void *>(TransformGetRotation));
    mono_add_internal_call("Pine.World.Components.Transform::GetScale", reinterpret_cast<void *>(TransformGetScale));
    mono_add_internal_call("Pine.World.Components.Transform::GetLocalPosition", reinterpret_cast<void *>(TransformGetLocalPosition));
    mono_add_internal_call("Pine.World.Components.Transform::SetLocalPosition", reinterpret_cast<void *>(TransformSetLocalPosition));
    mono_add_internal_call("Pine.World.Components.Transform::GetLocalRotation", reinterpret_cast<void *>(TransformGetLocalRotation));
    mono_add_internal_call("Pine.World.Components.Transform::SetLocalRotation", reinterpret_cast<void *>(TransformSetLocalRotation));
    mono_add_internal_call("Pine.World.Components.Transform::SetLocalEulerAngles", reinterpret_cast<void *>(TransformSetLocalEulerAngles));
    mono_add_internal_call("Pine.World.Components.Transform::GetLocalEulerAngles", reinterpret_cast<void *>(TransformGetLocalEulerAngles));
    mono_add_internal_call("Pine.World.Components.Transform::GetLocalScale", reinterpret_cast<void *>(TransformGetLocalScale));
    mono_add_internal_call("Pine.World.Components.Transform::SetLocalScale", reinterpret_cast<void *>(TransformSetLocalScale));
    mono_add_internal_call("Pine.World.Components.Transform::GetUp", reinterpret_cast<void *>(TransformGetUp));
    mono_add_internal_call("Pine.World.Components.Transform::GetRight", reinterpret_cast<void *>(TransformGetRight));
    mono_add_internal_call("Pine.World.Components.Transform::GetForward", reinterpret_cast<void *>(TransformGetForward));
}
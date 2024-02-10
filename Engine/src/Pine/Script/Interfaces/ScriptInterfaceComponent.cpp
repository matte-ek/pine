#include "Interfaces.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/World/Components/IComponent/IComponent.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Components/Components.hpp"
#include "mono/metadata/object-forward.h"
#include "mono/metadata/object.h"
#include <mono/metadata/appdomain.h>

namespace
{
    bool GetActive(std::uint32_t internalId, int type)
    {
        return Pine::Components::GetByInternalId(static_cast<Pine::ComponentType>(type), internalId)->GetActive();
    }

    void SetActive(std::uint32_t internalId, int type, bool active)
    {
        Pine::Components::GetByInternalId(static_cast<Pine::ComponentType>(type), internalId)->SetActive(active);
    }

    // -----------------------------------------------------

    void SetModel(std::uint32_t internalId, std::uint32_t assetId)
    {
        dynamic_cast<Pine::ModelRenderer*>(Pine::Components::GetByInternalId(Pine::ComponentType::ModelRenderer, internalId))->SetModel(
            dynamic_cast<Pine::Model*>(Pine::Assets::GetById(assetId))
        );
    }

    MonoObject* GetModel(std::uint32_t internalId)
    {
        return mono_gchandle_get_target(dynamic_cast<Pine::ModelRenderer*>(Pine::Components::GetByInternalId(Pine::ComponentType::ModelRenderer, internalId))->GetModel()->GetScriptHandle()->Handle);
    }

    // -----------------------------------------------------

    void GetPosition(std::uint32_t internalId, Pine::Vector3f *position)
    {
        *position = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->LocalPosition;
    }

    void SetPosition(std::uint32_t internalId, Pine::Vector3f *position)
    {
        Pine::Components::GetByInternalId<Pine::Transform>(internalId)->LocalPosition = *position;
    }

    void GetRotation(std::uint32_t internalId, Pine::Quaternion *rotation)
    {
        *rotation = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->LocalRotation;
    }

    void SetRotation(std::uint32_t internalId, Pine::Quaternion *rotation)
    {
        Pine::Components::GetByInternalId<Pine::Transform>(internalId)->LocalRotation = *rotation;
    }

    void GetScale(std::uint32_t internalId, Pine::Vector3f *scale)
    {
        *scale = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->LocalScale;
    }

    void SetScale(std::uint32_t internalId, Pine::Vector3f *scale)
    {
        Pine::Components::GetByInternalId<Pine::Transform>(internalId)->LocalScale = *scale;
    }

    void GetUp(std::uint32_t internalId, Pine::Vector3f *up)
    {
        *up = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->GetUp();
    }

    void GetRight(std::uint32_t internalId, Pine::Vector3f *right)
    {
        *right = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->GetRight();
    }

    void GetForward(std::uint32_t internalId, Pine::Vector3f *forward)
    {
        *forward = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->GetForward();
    }

    // -----------------------------------------------------
}

void Pine::Script::Interfaces::Component::Setup()
{
    mono_add_internal_call("Pine.World.Component::GetActive", reinterpret_cast<void *>(GetActive));
    mono_add_internal_call("Pine.World.Component::SetActive", reinterpret_cast<void *>(SetActive));
    mono_add_internal_call("Pine.World.Components.ModelRenderer::SetModel", reinterpret_cast<void *>(SetModel));
    mono_add_internal_call("Pine.World.Components.ModelRenderer::GetModel", reinterpret_cast<void *>(GetModel));
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
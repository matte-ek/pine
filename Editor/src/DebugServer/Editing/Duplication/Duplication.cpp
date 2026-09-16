#include "Duplication.hpp"

#include <set>

#include "../Values/Values.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"

bool Editor::DebugServer::Editing::Duplication::Supports(const Pine::ComponentType type)
{
    // Copying is a separate lifecycle capability from exposing writable properties.
    return type == Pine::ComponentType::Transform || type == Pine::ComponentType::ModelRenderer
        || type == Pine::ComponentType::Light || type == Pine::ComponentType::Camera
        || type == Pine::ComponentType::Collider || type == Pine::ComponentType::RigidBody;
}

Editor::DebugServer::Editing::Duplication::EntityState
Editor::DebugServer::Editing::Duplication::Read(const Pine::Entity* entity)
{
    EntityState state;
    state.Name = entity->GetName();
    state.Active = entity->GetActive();
    state.Static = entity->GetStatic();
    state.Tags = entity->GetTags();

    for (const auto component : entity->GetComponents())
    {
        ComponentState copied;
        copied.Type = component->GetType();
        copied.SourceId = component->GetId();
        copied.Active = component->GetActive();
        if (Supports(copied.Type))
        {
            copied.Properties = Components::Find(copied.Type)->Read(component);
        }
        if (copied.Type == Pine::ComponentType::ModelRenderer)
        {
            const auto renderer = static_cast<const Pine::ModelRenderer*>(component);
            copied.OverrideStencilBuffer = renderer->GetOverrideStencilBuffer();
            copied.StencilBufferValue = renderer->GetStencilBufferValue();
        }
        if (copied.Type == Pine::ComponentType::Camera)
        {
            const auto camera = static_cast<const Pine::Camera*>(component);
            copied.CameraClearColor = camera->GetClearColor();
            copied.CameraOverrideAspectRatio = camera->GetOverrideAspectRatio();
            copied.CameraOrthographicSize = camera->GetOrthographicSize();
        }
        state.Components.push_back(std::move(copied));
    }
    return state;
}

void Editor::DebugServer::Editing::Duplication::Validate(EntityState& state, const std::string& path)
{
    Values::Require(!state.Components.empty() && state.Components.front().Type == Pine::ComponentType::Transform,
        path, "Duplicated entities must have their required Transform first.");
    std::set<Pine::ComponentType> types;
    for (auto& component : state.Components)
    {
        Values::Require(Supports(component.Type), path,
            "The hierarchy contains a component without duplication support. See /edit/schema.");
        Values::Require(types.insert(component.Type).second, path,
            "Cannot duplicate an entity with repeated component types.");
        const auto adapter = Components::Find(component.Type);
        component.Properties = Components::Prepare(*adapter, adapter->Read(nullptr), component.Properties, path);
    }
}

void Editor::DebugServer::Editing::Duplication::ApplyEntity(Pine::Entity* entity, const EntityState& state)
{
    entity->SetName(state.Name);
    entity->SetActive(state.Active);
    entity->SetStatic(state.Static);
    entity->SetTags(state.Tags);
    entity->SetDirty(true);
}

void Editor::DebugServer::Editing::Duplication::ApplyComponent(Pine::Component* component, const ComponentState& state)
{
    Components::Find(state.Type)->Apply(component, state.Properties);
    component->SetActive(state.Active);
    if (state.Type == Pine::ComponentType::ModelRenderer)
    {
        const auto renderer = static_cast<Pine::ModelRenderer*>(component);
        renderer->SetOverrideStencilBuffer(state.OverrideStencilBuffer);
        renderer->SetStencilBufferValue(state.StencilBufferValue);
    }
    if (state.Type == Pine::ComponentType::Camera)
    {
        const auto camera = static_cast<Pine::Camera*>(component);
        camera->SetClearColor(state.CameraClearColor);
        camera->SetOverrideAspectRatio(state.CameraOverrideAspectRatio);
        camera->SetOrthographicSize(state.CameraOrthographicSize);
    }
}

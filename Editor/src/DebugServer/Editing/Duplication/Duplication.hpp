#pragma once

#include "../Components/Components.hpp"
#include "Pine/World/Entity/Entity.hpp"

namespace Editor::DebugServer::Editing::Duplication
{
    struct ComponentState
    {
        Pine::ComponentType Type;
        Pine::UId SourceId;
        nlohmann::json Properties;
        bool Active = true;
        bool OverrideStencilBuffer = false;
        int StencilBufferValue = 0xFF;
        Pine::Vector4f CameraClearColor = Pine::Vector4f(0.f);
        float CameraOverrideAspectRatio = 0.f;
        float CameraOrthographicSize = 1.f;
    };

    struct EntityState
    {
        std::string Name;
        bool Active = true;
        bool Static = false;
        std::uint64_t Tags = 0;
        std::vector<ComponentState> Components;
    };

    // Unsupported types are retained so validation can reject the entire hierarchy.
    bool Supports(Pine::ComponentType type);
    EntityState Read(const Pine::Entity* entity);
    void Validate(EntityState& state, const std::string& path);
    void ApplyEntity(Pine::Entity* entity, const EntityState& state);
    void ApplyComponent(Pine::Component* component, const ComponentState& state);
}

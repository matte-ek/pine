#pragma once

#include "../DebugServer.hpp"

namespace Pine
{
    class Entity;
    class Component;
}

namespace Editor::DebugServer::Editing
{
    Response GetSchema(const Request& request);
    Response Edit(const Request& request);

    // Live values in the edit format; null for protected entities or unsupported components.
    nlohmann::json ReadEntityProperties(const Pine::Entity* entity);
    nlohmann::json ReadComponentProperties(const Pine::Component* component);

    // Shared by batch execution and history restoration; cleans selection, drags and cameras.
    nlohmann::json DeleteEntityHierarchy(Pine::Entity* entity);
    bool RemoveSceneComponent(Pine::Component* component);
}

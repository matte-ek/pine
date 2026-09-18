#pragma once

#include "../DebugServer.hpp"

namespace Pine
{
    class Entity;
}

namespace Editor::DebugServer::Inspection
{
    // Shared identity fields for hierarchy, individual and batched inspection.
    nlohmann::json ReadIdentity(const Pine::Entity* entity);
    Response Query(const Request& request);
}

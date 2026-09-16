#pragma once

#include "../DebugServer.hpp"

namespace Editor::DebugServer::Endpoints
{
    // Registers every debug endpoint. Called once, from DebugServer::Setup().
    void Register();
    Response ReadEntity(const Request& request);
}

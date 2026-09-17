#pragma once

#include "../../DebugServer.hpp"

namespace Editor::DebugServer::Spatial::Queries
{
    Response Overlap(const Request& request);
    Response Raycast(const Request& request);
}

#pragma once

#include "../DebugServer.hpp"

namespace Editor::DebugServer::Camera
{
    Response Get(const Request& request);
    Response Set(const Request& request);
    Response Frame(const Request& request);
}

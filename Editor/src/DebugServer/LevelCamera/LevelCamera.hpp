#pragma once

#include "../DebugServer.hpp"

namespace Editor::DebugServer::LevelCamera
{
    Response Get(const Request& request);
    Response Set(const Request& request);
    nlohmann::json Schema();
}

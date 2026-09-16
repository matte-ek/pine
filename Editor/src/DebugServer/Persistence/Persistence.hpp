#pragma once

#include "../DebugServer.hpp"

namespace Editor::DebugServer::Persistence
{
    Response Get(const Request& request);
    Response Save(const Request& request);
    Response SaveAs(const Request& request);
}

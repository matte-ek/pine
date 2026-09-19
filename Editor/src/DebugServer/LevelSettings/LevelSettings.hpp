#pragma once

#include "../DebugServer.hpp"

namespace Pine
{
    struct LevelSettings;
}

namespace Editor::DebugServer::LevelSettings
{
    Response Get(const Request& request);
    Response Set(const Request& request);
    nlohmann::json Schema();

    // The advertised properties of one settings struct, and the reverse. Shared with the history
    // snapshot so undo compares and restores exactly the fields this route can write.
    nlohmann::json Read(const Pine::LevelSettings& settings);
    void Apply(Pine::LevelSettings& settings, const nlohmann::json& state);
}

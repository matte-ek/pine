#pragma once

#include <map>
#include "../Duplication/Duplication.hpp"
#include "../../DebugServer.hpp"

namespace Editor::DebugServer::Editing::History
{
    struct EntityState
    {
        Duplication::EntityState State;
        Pine::UId Parent;
        std::vector<Pine::UId> Children;
    };

    struct Snapshot
    {
        std::map<std::string, EntityState> Entities;
        std::vector<Pine::UId> Order;
        Pine::UId GameCamera;
        bool RestoreGameCamera = false;

        // The active Level's advertised atmosphere properties, in the wire form
        // LevelSettings::Read produces. Tracked as JSON so a snapshot covers exactly the fields
        // /level/settings can write, and nothing else in the Level asset.
        nlohmann::json Settings;
        bool RestoreSettings = false;
    };

    Snapshot Capture();
    void Record(Snapshot before, Snapshot after);

    Response Get(const Request& request);
    Response Undo(const Request& request);
    Response Redo(const Request& request);
}

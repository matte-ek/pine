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
    };

    Snapshot Capture();
    void Record(Snapshot before, Snapshot after);

    Response Get(const Request& request);
    Response Undo(const Request& request);
    Response Redo(const Request& request);
}

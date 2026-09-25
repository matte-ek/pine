#pragma once

#include <vector>

#include "Pine/World/Entity/Entity.hpp"

// The editor's entity copy, paste and duplicate, behind the Edit menu, the entity list's context
// menu and Ctrl+C / Ctrl+V / Ctrl+D.
//
// A copy is a snapshot of each entity and its children, taken at the moment of copying, so the
// originals can be edited or deleted afterwards (as Cut does) without changing what gets pasted.
namespace Editor::Clipboard::Entity
{
    // An entity whose parent, or any ancestor further up, is also being copied comes along inside
    // that ancestor's copy rather than being copied a second time on its own.
    void Copy(const std::vector<Pine::Entity*>& entities);

    bool HasData();

    // Spawns a new copy of every copied entity, under the parent its original had when it was copied
    // if that parent still exists. The clipboard keeps its contents, so pasting again copies again.
    // Records one undo step.
    std::vector<Pine::Entity*> Paste();

    // Copy and paste in one, without touching what the clipboard holds. Records one undo step.
    std::vector<Pine::Entity*> Duplicate(const std::vector<Pine::Entity*>& entities);
}

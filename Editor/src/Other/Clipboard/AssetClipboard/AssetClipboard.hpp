#pragma once

#include <vector>

#include "Pine/Assets/Asset/Asset.hpp"

// The editor's asset copy, behind the Edit menu and Ctrl+C.
//
// Nothing pastes assets yet. This only remembers which assets were copied, by id, so that an
// asset deleted or unloaded after the copy drops out instead of leaving a dangling pointer.
namespace Editor::Clipboard::Asset
{
    void Copy(const std::vector<Pine::Asset*>& assets);

    bool HasData();

    // The copied assets that still exist, in the order they were copied.
    std::vector<Pine::Asset*> GetAssets();
}

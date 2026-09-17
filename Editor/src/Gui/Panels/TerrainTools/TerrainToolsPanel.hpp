#pragma once

#include "Other/TerrainSculpting/TerrainSculpting.hpp"

// The sculpting brush's settings, and the switch that hands the Level viewport over to it.
//
// The panel deliberately does not pick a terrain to edit. The ray from the cursor already says
// which terrain is under it, so choosing one here would be a second answer to a question that is
// already answered - and a wrong one whenever the author clicks on a different terrain.
namespace Panels::TerrainTools
{
    void SetActive(bool value);
    bool GetActive();

    // Whether the viewport should sculpt instead of selecting and moving entities. False whenever
    // the panel is closed, so closing it always gives the normal tools back.
    bool IsEditing();

    // The brush the viewport applies. Read once per stroke step rather than copied, so a slider
    // moved mid-drag takes effect immediately.
    const Editor::TerrainSculpting::Brush& GetBrush();

    void Render();
}

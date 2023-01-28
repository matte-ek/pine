#pragma once

#include "Pine/Core/Math/Math.hpp"

namespace Panels::LevelViewport
{
    void SetActive(bool value);
    bool GetActive();

    bool GetVisible();
    Pine::Vector2i GetSize();

    void Render();
}
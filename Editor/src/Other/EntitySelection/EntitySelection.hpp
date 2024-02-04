#pragma once

#include "Pine/Core/Math/Math.hpp"

namespace EntitySelection
{
    void Setup();
    void Dispose();

    void Pick(Pine::Vector2i cursorPosition);
}
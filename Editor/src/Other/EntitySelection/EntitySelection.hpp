#pragma once

#include "Pine/Core/Math/Math.hpp"

namespace Editor::Gui::EntitySelection
{
    void Setup();
    void Dispose();

    void Pick(Pine::Vector2i cursorPosition, bool pickMultiple);
}
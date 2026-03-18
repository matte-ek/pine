#pragma once

struct ImFont;

namespace Editor::Gui
{
    ImFont* GetBoldFont();

    void Setup();
    void Shutdown();
}

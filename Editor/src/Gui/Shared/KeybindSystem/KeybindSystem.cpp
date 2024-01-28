#include "KeybindSystem.hpp"
#include "Pine/Input/Input.hpp"
#include "imgui.h"

namespace
{
    struct KeyBind
    {
        std::string Name;

        int Key = 0;

        bool Ctrl = false;
        bool Alt = false;
    };

    std::vector<KeyBind> Keybinds;
}

std::uint32_t KeybindSystem::RegisterKeybind(const std::string &name, int key, bool ctrl, bool alt)
{
    KeyBind keybind;

    keybind.Name = name;
    keybind.Key = key;
    keybind.Ctrl = ctrl;
    keybind.Alt = alt;

    Keybinds.push_back(keybind);

    return Keybinds.size() - 1;
}

bool KeybindSystem::IsKeybindPressed(std::uint32_t id)
{
    const auto& io = ImGui::GetIO();
    const auto& keybind = Keybinds[id];

    if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
        return false;

    if (keybind.Ctrl && !io.KeyCtrl)
        return false;
    if (keybind.Alt && !io.KeyAlt)
        return false;

    return ImGui::IsKeyPressed(static_cast<ImGuiKey>(keybind.Key));
}

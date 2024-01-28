#pragma once
#include <string>
#include <cstdint>

namespace KeybindSystem
{
    std::uint32_t RegisterKeybind(const std::string& name, int key, bool ctrl = false, bool alt = false);
    bool IsKeybindPressed(std::uint32_t id);
}
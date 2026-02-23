#pragma once
#include <string>

namespace Editor::Utilities::Script
{
    void Setup();

    void AddScript(const std::string& filePath);
    void DeleteScript(const std::string& filePath);
}

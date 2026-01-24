#pragma once
#include <string>

namespace ScriptingUtilities
{
    void Setup();

    void AddScript(const std::string& filePath);
    void DeleteScript(const std::string& filePath);
}

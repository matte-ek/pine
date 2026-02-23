#pragma once
#include <filesystem>
#include <string>

namespace Editor::Utilities::Asset
{
    // Will return a mapped string when working with "working directories" within the asset system.
    std::string EstimateMappedPath(std::filesystem::path path, const std::string& relativePath);

    // Will attempt to save any changed project asset(s) to their file
    void SaveAll();
}

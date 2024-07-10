#pragma once

#include <filesystem>

namespace Editor::OsNative
{
    void OpenFileExplorer(const std::filesystem::path& path);
}
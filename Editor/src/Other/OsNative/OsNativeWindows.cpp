#include "OsNative.hpp"

#ifdef _WIN64

#include <Windows.h>

void Editor::OsNative::OpenFileExplorer(const std::filesystem::path &path)
{
    const auto& pathString = path.string();
    const auto wStr = std::wstring(pathString.begin(), pathString.end());

    ShellExecuteW(nullptr, L"open", static_cast<LPCWSTR>(wStr.c_str()), nullptr, nullptr, SW_SHOWNORMAL);
}

#endif
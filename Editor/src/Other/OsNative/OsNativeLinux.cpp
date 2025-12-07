#include "OsNative.hpp"

#ifdef __linux__

void Editor::OsNative::OpenFileExplorer(const std::filesystem::path& path)
{
    system(std::string("xdg-open " + path.string()).c_str());
}

#endif

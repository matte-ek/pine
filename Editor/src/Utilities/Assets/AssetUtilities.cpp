#include "AssetUtilities.hpp"

#include "Pine/Core/File/File.hpp"
#include "Pine/Core/String/String.hpp"

std::string Editor::Utilities::Asset::EstimateMappedPath(std::filesystem::path path, const std::string& relativePath)
{
    auto pathStr = Pine::File::UniversalPath(path.replace_extension("").string());

    if (relativePath.empty())
    {
        return pathStr;
    }

    if (Pine::String::StartsWith(pathStr, relativePath))
    {
        return pathStr.substr(relativePath.length());
    }

    return pathStr;
}

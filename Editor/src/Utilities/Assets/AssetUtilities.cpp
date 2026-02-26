#include "AssetUtilities.hpp"

#include <set>

#include "Gui/Panels/AssetBrowser/AssetBrowserPanel.hpp"
#include "Gui/Panels/AssetBrowser/AssetHierarchy/AssetHierarchy.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Core/File/File.hpp"
#include "Pine/Core/String/String.hpp"
#include "Projects/Projects.hpp"

std::string Editor::Utilities::Asset::EstimateMappedPath(std::filesystem::path path, const std::string& relativePath)
{
    auto pathStr = Pine::File::UniversalPath(path.replace_extension("").string());

    if (relativePath.empty())
    {
        return pathStr;
    }

    if (Pine::String::StartsWith(pathStr, relativePath))
    {
        pathStr = pathStr.substr(relativePath.length());

        if (pathStr.front() == '/')
        {
            pathStr = pathStr.substr(1);
        }
    }

    return pathStr;
}

Pine::Asset* Editor::Utilities::Asset::CreateEmptyAsset(const std::filesystem::path& path, Pine::AssetType type)
{
    // Create an empty version of the asset type.
    auto asset = Pine::Assets::CreateAsset(type, path.string());
    asset->SaveToFile();
    delete asset;

    // Use the asset manager to load the newly created asset
    asset = Pine::Assets::LoadAssetFromFile(std::filesystem::path(EstimateMappedPath(path, Pine::Assets::Internal::GetWorkingDirectory())).replace_extension(".passet").string());

    return asset;
}

void Editor::Utilities::Asset::ImportAsset(const std::string& contentFile)
{
    auto currentDirectory = Panels::AssetBrowser::GetOpenDirectoryNode();

    static std::set<std::string> defaultImporterSupportedExtensions = {
        ".png",
        ".jpeg",
        ".jpg",
        ".fbx",
        ".glb",
        ".dae",
        ".ih"
    };

    auto filePath = std::filesystem::path(contentFile);
    auto extension = Pine::String::ToLower(filePath.extension().string());

    if (defaultImporterSupportedExtensions.count(extension) == 0)
    {
        Pine::Log::Error(fmt::format("Could not import asset '{}', file format not supported.", contentFile));
        return;
    }

    const auto assetFilePath = currentDirectory->Path.string() + "/" + filePath.stem().string();
    const auto contentFileName = Pine::String::Replace(EstimateMappedPath(assetFilePath, Pine::Assets::Internal::GetWorkingDirectory()), "/", "-");
    const auto contentFilePath = Projects::GetProjectPath() + "/content/" + contentFileName + filePath.extension().string();

    if (std::filesystem::exists(contentFilePath))
    {
        std::filesystem::remove(contentFilePath);
    }

    // Copy the source file into the projects content directory
    std::filesystem::copy(filePath, contentFilePath);

    auto asset = Pine::Assets::ImportAssetFromFile(
        contentFilePath,
        assetFilePath);

    if (!asset)
    {
        Pine::Log::Error(fmt::format("Could not import asset '{}', engine import failed.", contentFile));
        return;
    }

    asset->SaveToFile();

    delete asset;
}

void Editor::Utilities::Asset::ImportAssets(const std::vector<std::string>& paths)
{
    for (const auto& path : paths)
    {
        if (std::filesystem::is_directory(path))
        {
            for (const auto& iter : std::filesystem::recursive_directory_iterator(path))
            {
                if (!iter.is_regular_file())
                {
                    continue;
                }

                ImportAsset(iter.path().string());
            }
        }
        else
        {
            ImportAsset(path);
        }
    }
}

void Editor::Utilities::Asset::RefreshAll()
{
    Projects::LoadProjectAssets();
    Panels::AssetBrowser::BuildAssetHierarchy();
}

void Editor::Utilities::Asset::SaveAll()
{
    for (const auto& [uid, asset] : Pine::Assets::GetAll())
    {
        if (!asset->HasBeenModified())
        {
            continue;
        }

        if (asset->GetFilePath().empty())
        {
            continue;
        }

        asset->SaveToFile();
    }
}

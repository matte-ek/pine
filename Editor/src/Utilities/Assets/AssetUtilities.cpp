#include "AssetUtilities.hpp"

#include <set>

#include "Gui/Panels/AssetBrowser/AssetBrowserPanel.hpp"
#include "Gui/Panels/AssetBrowser/AssetHierarchy/AssetHierarchy.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Importer/AssetImporter.hpp"
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

    return Pine::String::ToLower(pathStr);
}

Pine::Asset* Editor::Utilities::Asset::CreateEmptyAsset(const std::filesystem::path& path, Pine::AssetType type)
{
    // Create an empty version of the asset type.
    auto asset = Pine::Assets::CreateAsset(type, path.string());
    asset->SaveToFile();
    delete asset;

    // Use the asset manager to load the newly created asset
    asset = Pine::Assets::LoadAssetFromFile(std::filesystem::path(EstimateMappedPath(path, Pine::Assets::Internal::GetWorkingDirectory())).replace_extension(".passet").string());

    // Make sure this new asset will appear in the browser.
    Panels::AssetBrowser::BuildAssetHierarchy();

    return asset;
}

void Editor::Utilities::Asset::ImportAssets(const std::vector<std::string>& paths)
{
    auto importContext = Pine::Importer::CreateContext();
    auto currentDirectory = Panels::AssetBrowser::GetOpenDirectoryNode();

    importContext->CopySourceFiles = true;
    importContext->ContentPath = Projects::GetProjectPath() + "/content";

    for (const auto& path : paths)
    {
        if (std::filesystem::is_regular_file(path))
        {
            auto relativePath = path.substr(std::filesystem::path(path).parent_path().string().length() + 1);

            Pine::Importer::AddFile(importContext, path, currentDirectory->Path.string() + "/" + relativePath);

            continue;
        }

        for (const auto& iter : std::filesystem::recursive_directory_iterator(path))
        {
            if (iter.is_directory())
            {
                continue;
            }

            auto relativePath = iter.path().string().substr(std::filesystem::path(path).parent_path().string().length() + 1);

            Pine::Importer::AddFile(importContext, iter.path().string(), currentDirectory->Path.string() + "/" + relativePath);
        }
    }

    Pine::Importer::Run(importContext);

    int importedAssets = 0;
    int failedAssets = 0;

    for (const auto& iter : importContext->Imports)
    {
        if (iter.ImportStatus == Pine::AssetImportStatus::Imported)
        {
            importedAssets++;
        }
        else if (iter.ImportStatus == Pine::AssetImportStatus::Failed)
        {
            failedAssets++;
        }
    }

    PInfo(fmt::format("Imported {} assets", importedAssets));

    Pine::Importer::DeleteContext(importContext);
}

void Editor::Utilities::Asset::DeletePath(const std::filesystem::path& path)
{
    if (std::filesystem::is_directory(path))
    {
        for (const auto& iter : std::filesystem::directory_iterator(path))
        {
            DeletePath(iter.path());
        }
    }
    else
    {
        // Translate file path to an internal asset path
        const auto intPath = Pine::String::ToLower(Pine::String::StartsWith(path, Pine::Assets::Internal::GetWorkingDirectory()) ?
            path.string().substr(Pine::Assets::Internal::GetWorkingDirectory().length()) : path.string());

        const auto asset = Pine::Assets::GetAssetByPath(std::filesystem::path(intPath).replace_extension("").string());
        if (!asset)
        {
            PWarning("Editor: Could not find asset by file path during deletion.");
            return;
        }

        Pine::Assets::Internal::DeleteAsset(asset);
    }

    std::filesystem::remove(path.string());
}

void Editor::Utilities::Asset::RefreshAll()
{
    // Load in any new assets in the project directory
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

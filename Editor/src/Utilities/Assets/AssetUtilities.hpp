#pragma once
#include <filesystem>
#include <string>

#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Assets/Importer/AssetImporter.hpp"

namespace Editor::Utilities::Asset
{
    // Will return a mapped string when working with "working directories" within the asset system.
    std::string EstimateMappedPath(std::filesystem::path path, const std::string& relativePath);

    // Will create and save an empty asset of the specified type to disk, and then
    // load that asset into the asset manager, and return that newly loaded asset.
    Pine::Asset* CreateEmptyAsset(const std::filesystem::path& path, Pine::AssetType type);

    // Builds an import context for the given files and directories, targeting the directory
    // currently open in the asset browser. The queue is filled but nothing is resolved or
    // imported: that is the caller's to drive, and the caller owns the context - see
    // Pine::Importer::DeleteContext().
    Pine::Importer::ImportContext* CreateImportContext(const std::vector<std::string>& paths);

    // Same import flow with an explicit filesystem destination instead of the browser selection.
    Pine::Importer::ImportContext* CreateImportContext(const std::vector<std::string>& paths,
        const std::filesystem::path& destinationDirectory);

    // Utilities to delete both assets and directories
    void DeletePath(const std::filesystem::path& path);

    // Will reload any changed assets, but also load in any new assets.
    void RefreshAll();

    // Will attempt to save any changed project asset(s) to their file
    void SaveAll();
}

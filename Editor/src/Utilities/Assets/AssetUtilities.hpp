#pragma once
#include <filesystem>
#include <string>

#include "Pine/Assets/Asset/Asset.hpp"

namespace Editor::Utilities::Asset
{
    // Will return a mapped string when working with "working directories" within the asset system.
    std::string EstimateMappedPath(std::filesystem::path path, const std::string& relativePath);

    // Will create and save an empty asset of the specified type to disk, and then
    // load that asset into the asset manager, and return that newly loaded asset.
    Pine::Asset* CreateEmptyAsset(const std::filesystem::path& path, Pine::AssetType type);

    // Utilities to import assets into Pine
    void ImportAsset(const std::string& contentFile);
    void ImportAssets(const std::vector<std::string>& paths);

    // Will reload any changed assets, but also load in any new assets.
    void RefreshAll();

    // Will attempt to save any changed project asset(s) to their file
    void SaveAll();
}

#pragma once
#include "Pine/Assets/Asset/Asset.hpp"

#include <filesystem>

namespace Pine::Assets
{
    /// ---------------------------------------------------------------------------------------------------
    /// General
    /// ---------------------------------------------------------------------------------------------------

    void Setup();
    void Shutdown();

    /// ---------------------------------------------------------------------------------------------------
    /// Loading and importing assets
    /// ---------------------------------------------------------------------------------------------------

    // Sets a path prefix for the following asset loading operations.
    void SetWorkingDirectory(std::string_view workingDirectory);

    // Attempts to load a single pine asset file specified.
    Asset* LoadAssetFromFile(const std::filesystem::path& filePath);

    // Attempts to load all pine asset files recursively in the specified directory.
    // An empty string will load everything in the current working directory.
    int LoadAssetsFromDirectory(const std::filesystem::path& directory);

    // Attempts to import a source file into a pine asset.
    Asset* ImportAssetFromFile(const std::filesystem::path& sourceFilePath, std::string_view mappedPath);

    // Attempts to load multiple source files into a pine asset. This is used
    // for assets which contain multiple source files, e.g. shaders.
    Asset* ImportAssetFromFiles(const std::vector<std::filesystem::path>& sourceFilePaths, std::string_view mappedPath);

    /// ---------------------------------------------------------------------------------------------------
    /// Creating and saving assets
    /// ---------------------------------------------------------------------------------------------------

    Asset* CreateAsset(AssetType type, std::string_view assetPath);

    /// ---------------------------------------------------------------------------------------------------
    /// Accessing assets
    /// ---------------------------------------------------------------------------------------------------

    Asset* GetAssetByUId(UId id);
    Asset* GetAssetByPath(std::string_view path);

    template<typename TAsset>
    TAsset* Get(UId id)
    {
        return GetAssetByUId(id);
    }

    template<typename TAsset>
    TAsset* Get(std::string_view path)
    {
        return dynamic_cast<TAsset*>(GetAssetByPath(path));
    }

    const std::unordered_map<UId, Asset*>& GetAll();

    /// ---------------------------------------------------------------------------------------------------
    /// Internal methods for engine usage.
    /// ---------------------------------------------------------------------------------------------------

    namespace Internal
    {
        const std::string& GetWorkingDirectory();

        Asset* CreateAssetByType(AssetType type);
    }
}
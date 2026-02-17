#pragma once
#include "Pine/Assets/Asset/Asset.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>

namespace Pine
{

    enum class AssetManagerState
    {
        Idle,
        LoadFile,
        LoadDirectory
    };

}

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
    bool ImportAssetFromFile(const std::filesystem::path& sourceFilePath, std::string_view outputFilePath = "");

    // Attempts to load multiple source files into a pine asset. This is used
    // for assets which contain multiple source files, e.g. shaders.
    bool ImportAssetFromFiles(const std::vector<std::filesystem::path>& sourceFilePaths, std::string_view mappedPath, std::string_view outputFilePath = "");

    /// ---------------------------------------------------------------------------------------------------
    /// Creating and saving assets
    /// ---------------------------------------------------------------------------------------------------

    Asset* CreateAsset(AssetType type, std::string_view assetPath);

    /// ---------------------------------------------------------------------------------------------------
    /// Accessing assets
    /// ---------------------------------------------------------------------------------------------------

    Asset* GetAssetByGuid(Guid id);
    Asset* GetAssetByPath(std::string_view path);

    template<typename TAsset>
    TAsset* Get(Guid id)
    {
        return GetAssetByGuid(id);
    }

    template<typename TAsset>
    TAsset* Get(std::string_view path)
    {
        return dynamic_cast<TAsset*>(GetAssetByPath(path));
    }

    /// ---------------------------------------------------------------------------------------------------
    /// Internal methods for engine usage.
    /// ---------------------------------------------------------------------------------------------------

    namespace Internal
    {
        // Gets what state the asset manager is in, such as if we're in the process of loading a directory.
        // Useful for parts of the engine to determine if assets can be added as an AssetResolveReference.
        AssetManagerState GetState();

        Asset* CreateAssetByType(AssetType type);
    }
}
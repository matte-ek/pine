#pragma once
#include "Pine/Assets/IAsset/IAsset.hpp"

#include <filesystem>
#include <string>

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
    void Setup();
    void Shutdown();

    // Attempts to load an asset from a file on disk, will guess the asset type
    // depending on the file extension. Will use the root path as the relative path
    // it's going to as the internal path. You can always overwrite this with mapPath.
    IAsset* LoadFromFile(const std::filesystem::path& filePath, const std::string& rootPath = "", const std::string& mapPath = "");

    // Attempts to recursively load all asset files from a directory. Will by default
    // map all assets as relative path to the specified path, however you can overwrite this
    // behaviour with useAsRelativePath. Returns the amount of assets that it FAILED to load, or -1 if none were loaded.
    int LoadDirectory(const std::filesystem::path& directoryPath, bool startIndex = true);

    // Resolve references are a way to reference assets that may not already have been loaded during the load stage.
    // This allows the asset manager to load assets efficiently and take care of the loose ends at the end of everything.
    void AddAssetResolveReference(const AssetResolveReference& resolveReference);

    // Attempts to find an already loaded asset with it's mapped path.
    IAsset* GetAsset(const std::string& path);

    template<typename T>
    T* GetAsset(const std::string& path)
    {
        return dynamic_cast<T*>(GetAsset(path));
    }

    // Gets what state the asset manager is in, such as if we're in the process of loading a directory.
    // Useful for parts of the engine to determine if assets can be added as an AssetResolveReference.
    AssetManagerState GetState();

}
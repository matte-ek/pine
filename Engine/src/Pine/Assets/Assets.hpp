#pragma once
#include "Pine/Assets/IAsset/IAsset.hpp"

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
    void Setup();
    void Shutdown();

    // Attempts to load an asset from a file on disk, will guess the asset type
    // depending on the file extension. Will use the root path as the relative path
    // it's going to as the internal path. You can always overwrite this with mapPath.
    IAsset* LoadFromFile(const std::filesystem::path& filePath, const std::string& rootPath = "", const std::string& mapPath = "");

    // Attempts to recursively load all asset files from a directory. Will by default
    // map all assets as relative path to the specified path, however you can overwrite this
    // behaviour with useAsRelativePath. Returns the amount of assets that it __FAILED__ to load, or -1 if none were loaded.
    int LoadDirectory(const std::filesystem::path& directoryPath, bool useAsRelativePath = true);

    // Resolve references are a way to "lazy load" assets, and will signal to the asset manager that we will need to load
    // these assets added here later. For example while loading a Material, you don't exactly need to know the texture data
    // to have a material, we just know that these X textures are bound to this material, so we'll allow the asset manager
    // to load these textures later.
    void AddAssetResolveReference(const AssetResolveReference& resolveReference);

    // Attempts to find an already loaded asset with it's mapped path.
    // includeFilePath allows you to find the asset by its file path instead of "fake" engine path.
    IAsset* Get(const std::string& path, bool includeFilePath = false, bool logWarning = true);

    template<typename T>
    T* Get(const std::string& path)
    {
        return dynamic_cast<T*>(Get(path));
    }

    IAsset* GetById(std::uint32_t id);

    IAsset* GetOrLoad(const std::string& inputPath, bool includeFilePath = false);

    // Moves an already existing asset to a new path, the newPath needs to be a file system path,
    // and the asset will use the same root path as the old path.
    void MoveAsset(IAsset* asset, const std::filesystem::path& newFilePath);

    // Returns the entire map used internally within the asset manager
    const std::unordered_map<std::string, IAsset*>& GetAll();

    // Saves all new asset data that has been modified to disk
    void SaveAll();

    // Monitor all loaded asset's files for changes
    void RefreshAll();

    // Gets what state the asset manager is in, such as if we're in the process of loading a directory.
    // Useful for parts of the engine to determine if assets can be added as an AssetResolveReference.
    AssetManagerState GetState();

    const std::string& GetDirectoryBase();

}
#pragma once
#include <filesystem>
#include <vector>

namespace Pine
{
    class Asset;

    struct AssetImportConfiguration
    {
    };

    enum class AssetImportStatus
    {
        Waiting = 0,
        Importing,
        Failed,
        Imported
    };
}

namespace Pine::Importer
{
    struct ImportContext;

    struct AssetImport
    {
        // How the asset path should be laid out relative to the working directory.
        std::string EnginePath;

        // Since assets tend to have the same name sometimes for models and textures, and pine
        // requires us to have a unique asset name, we detect this and add the extension to the end
        // of the new path when this is true.
        bool AvoidDuplicate = false;

        // Files used when importing the asset, most asset types will only require 1.
        std::vector<std::filesystem::path> SourcePaths;

        // Optional customized import configuration, use correct type for each asset.
        AssetImportConfiguration* Configuration;

        // If loaded, the asset pointer here will point to the loaded asset
        Asset* AssetPtr = nullptr;

        AssetImportStatus ImportStatus = AssetImportStatus::Waiting;

        ImportContext* Context = nullptr;
    };

    struct ImportContext
    {
        std::vector<AssetImport> Imports;

        // Optionally copy the source file to a "content" directory, to allow the user
        // to later easily reimport the asset with other settings.
        bool CopySourceFiles = false;
        std::filesystem::path ContentPath;

        // Used for standalone importers, does not try to load the asset afterwards
        // into memory.
        bool DontLoad = false;
    };

    ImportContext* CreateContext();
    void DeleteContext(const ImportContext* context);

    void AddFile(
        ImportContext* context,
        const std::filesystem::path& sourcePath,
        const std::string& enginePath,
        AssetImportConfiguration* configuration = nullptr);

    void Run(ImportContext* context);

    // Can be used by other importers when dealing with dependencies.
    Asset* ImportRelative(const AssetImport* assetImport, const std::string& filePath);
}

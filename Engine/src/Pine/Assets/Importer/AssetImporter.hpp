#pragma once
#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

#include "Pine/Assets/Asset/Asset.hpp"

namespace Pine
{
    // Base for the per-asset-type import settings. The settings themselves live on the asset
    // (and are serialized into its '.passet'), not on the import queue entry - so re-importing
    // an asset later uses the same settings it was imported with. The importer's job is only
    // to make the asset available for configuration before it compiles, see Resolve().
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

    // What Resolve() decided should happen to a queued file. Everything here is knowable before
    // any asset data is written, which is what lets a caller show (or change) the plan first.
    enum class AssetImportAction
    {
        // Resolve() hasn't looked at this entry yet.
        Undetermined,

        // Nothing exists at the destination, a new asset will be created.
        Create,

        // An asset is already loaded at the destination; it gets re-imported in place, keeping
        // its UId. Assets reference each other by UId, so this is the only correct way to import
        // over something that already exists.
        Update,

        // No importer is registered for this file type.
        Unsupported,

        // Something is already at the destination that we can't safely import over: a '.passet'
        // on disk that isn't loaded, or a loaded asset of a different type. Importing anyway
        // would mint a new UId over it and silently break every reference to it.
        Conflict
    };
}

namespace Pine::Importer
{
    struct ImportContext;

    struct AssetImport
    {
        // How the asset path should be laid out relative to the working directory, as passed to
        // AddFile(). Keeps its source extension; Resolve() derives ResolvedEnginePath from it.
        std::string EnginePath;

        // Since assets tend to have the same name sometimes for models and textures, and pine
        // requires us to have a unique asset name, we detect this and add the extension to the end
        // of the new path when this is true.
        bool AvoidDuplicate = false;

        // Files used when importing the asset, most asset types will only require 1.
        std::vector<std::filesystem::path> SourcePaths;

        // --- Filled in by Resolve() ---

        // The path the asset is actually created at: extension stripped, duplicate suffix applied.
        std::string ResolvedEnginePath;

        AssetType Type = AssetType::Invalid;

        AssetImportAction Action = AssetImportAction::Undetermined;

        // The asset this entry imports into, available from Resolve() onwards so its import
        // configuration can be read and changed before Execute() compiles it. For Action::Update
        // this is the asset already registered with the asset manager.
        Asset* AssetPtr = nullptr;

        // Whether the context is responsible for deleting AssetPtr. Ownership is handed off on a
        // successful import - to the asset manager, or to the caller when the context is DontLoad.
        bool OwnsAsset = false;

        AssetImportStatus ImportStatus = AssetImportStatus::Waiting;

        // Whether the asset has already been given the chance to work out its own import settings,
        // see ProposeImportSettings(). Only ever done once per entry, however many callers ask.
        bool SettingsProposed = false;

        ImportContext* Context = nullptr;
    };

    struct ImportContext
    {
        // Held by pointer so that entry addresses stay stable: ImportRelative() appends
        // dependencies to this list while Execute() is walking it.
        std::vector<std::unique_ptr<AssetImport>> Imports;

        // Optionally copy the source file to a "content" directory, to allow the user
        // to later easily reimport the asset with other settings.
        bool CopySourceFiles = false;
        std::filesystem::path ContentPath;

        // Used for standalone importers, does not try to load the asset afterwards
        // into memory.
        bool DontLoad = false;

        // How far ExecuteNext() has walked 'Imports'. Entries Resolve() rejected are stepped over
        // rather than skipped, so this is simply how many of the queued files have been dealt
        // with - which is what a caller showing progress wants to read.
        std::size_t ExecuteCursor = 0;
    };

    ImportContext* CreateContext();

    // Also deletes any asset the context still owns, i.e. everything that was resolved but never
    // successfully imported. Successfully imported assets have been handed off by then.
    void DeleteContext(ImportContext* context);

    void AddFile(
        ImportContext* context,
        const std::filesystem::path& sourcePath,
        const std::string& enginePath);

    // Same as AddFile, but for assets built from several source files (e.g. a shader
    // with separate vertex/fragment sources) that must import into a single asset.
    void AddFiles(
        ImportContext* context,
        const std::vector<std::filesystem::path>& sourcePaths,
        const std::string& enginePath);

    // Decides what will happen to every queued file - type, destination, and whether it creates,
    // updates or collides with something - and constructs the (still empty) asset for each. Writes
    // nothing outside of memory, so it's safe to run and then throw away.
    void Resolve(ImportContext* context);

    // Lets each resolved asset work out import settings from what it can see of its source files,
    // see Asset::ResolveImportSettings().
    //
    // Separate from Resolve() because the two differ in what they touch. Resolve() only decides;
    // this changes an asset's settings, and for an Action::Update entry that asset is a live,
    // loaded one - so a caller that means to show the plan before committing to it gets to
    // snapshot first. Execute() calls this itself, so callers that don't care can ignore it.
    void ProposeImportSettings(ImportContext* context);

    // Compiles and commits everything Resolve() marked as Create or Update.
    void Execute(ImportContext* context);

    // Deals with a single queued file and advances the context's cursor past it. Returns false
    // once the cursor has reached the end of the queue and there was nothing left to do.
    //
    // This is Execute() one step at a time, so that a caller can keep painting - and draw
    // progress - while a large import runs. Pacing is the caller's business: the importer has no
    // idea what a frame is, and a single step can take as long as one BC7 texture takes.
    bool ExecuteNext(ImportContext* context);

    // Resolve() followed by Execute(), for callers that don't need to look at the plan.
    void Run(ImportContext* context);

    // Can be used by other importers when dealing with dependencies. 'configure' runs on the
    // dependency's asset after it is resolved but before it compiles, which is where an importer
    // that knows what a file is for (a model knowing a texture is its normal map) says so. It runs
    // for re-imports as well; what stops it from overwriting a setting the user changed by hand is
    // the setting's own record of how it was decided, not whether we call this. Return true
    // when settings change so an already compiled dependency can be rebuilt.
    Asset* ImportRelative(
        const AssetImport* assetImport,
        const std::string& filePath,
        const std::string& overrideFileName = "",
        const std::function<bool(Asset*)>& configure = nullptr);
}

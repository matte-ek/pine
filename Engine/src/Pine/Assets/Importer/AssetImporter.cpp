#include "AssetImporter.hpp"
#include "../Assets.hpp"
#include "Pine/Core/File/File.hpp"
#include "Pine/Core/String/String.hpp"

namespace
{
    using namespace Pine::Importer;

    std::string GenerateContentPath(
        const ImportContext* context,
        const std::string& enginePath)
    {
        const auto universalPath = Pine::File::UniversalPath(enginePath);

        const auto workingDirectoryRelPath = Pine::String::StartsWith(universalPath, Pine::Assets::Internal::GetWorkingDirectory()) ?
                                                 universalPath.substr(Pine::Assets::Internal::GetWorkingDirectory().length()) : universalPath;

        const auto suggestedContentFileName = Pine::String::Replace(workingDirectoryRelPath, "/", "-");

        return context->ContentPath.string() + "/" + suggestedContentFileName;
    }

    // The mapped ("virtual") path Asset::SetupNew() would give an asset created at this engine
    // path. Kept in sync with SetupNew by hand, since Resolve() needs to know where the asset
    // lands before there is an asset to ask.
    std::string GenerateMappedPath(const std::string& resolvedEnginePath)
    {
        const auto universalPath = Pine::File::UniversalPath(resolvedEnginePath);
        const auto& workingDirectory = Pine::Assets::Internal::GetWorkingDirectory();

        return Pine::String::ToLower(Pine::String::StartsWith(universalPath, workingDirectory) ?
            universalPath.substr(workingDirectory.length()) : universalPath);
    }

    void Resolve(const ImportContext* context, AssetImport& import)
    {
        if (import.Action != Pine::AssetImportAction::Undetermined)
        {
            return;
        }

        if (import.SourcePaths.empty())
        {
            import.Action = Pine::AssetImportAction::Unsupported;
            import.ImportStatus = Pine::AssetImportStatus::Failed;
            return;
        }

        import.Type = Pine::Assets::Internal::GetAssetTypeByFile(import.SourcePaths.front());

        if (import.Type == Pine::AssetType::Invalid)
        {
            PWarning(fmt::format("No importer for file {}, ignoring.", import.SourcePaths.front().string()));

            import.Action = Pine::AssetImportAction::Unsupported;
            import.ImportStatus = Pine::AssetImportStatus::Failed;
            return;
        }

        // Work out where this ends up before creating anything, so we can tell an import apart
        // from a re-import.
        auto resolvedEnginePath = std::filesystem::path(import.EnginePath).replace_extension("").string();

        if (import.AvoidDuplicate)
        {
            resolvedEnginePath += Pine::String::Replace(import.SourcePaths.front().extension().string(), ".", "-");
        }

        import.ResolvedEnginePath = resolvedEnginePath;

        // Is there already an asset living there? Importing over it by creating a new asset would
        // mint a new UId, and everything referencing the old one (materials, levels, components)
        // references it by UId. So re-import the existing asset in place instead.
        if (!context->DontLoad)
        {
            if (const auto existingAsset = Pine::Assets::GetAssetByPath(GenerateMappedPath(resolvedEnginePath)))
            {
                if (existingAsset->GetType() != import.Type)
                {
                    PError(fmt::format("Cannot import {} over the existing {} asset at {}.",
                        import.SourcePaths.front().string(), AssetTypeToString(existingAsset->GetType()), existingAsset->GetPath()));

                    import.Action = Pine::AssetImportAction::Conflict;
                    import.ImportStatus = Pine::AssetImportStatus::Failed;
                    return;
                }

                import.Action = Pine::AssetImportAction::Update;
                import.AssetPtr = existingAsset;
                import.OwnsAsset = false;

                return;
            }
        }

        // Nothing is loaded there, but there may still be a '.passet' on disk we'd overwrite -
        // one belonging to another project, or one that failed to load. Refuse rather than
        // silently reminting its UId.
        const auto targetFilePath = std::filesystem::path(Pine::File::UniversalPath(resolvedEnginePath)).replace_extension(".passet");

        if (std::filesystem::exists(targetFilePath))
        {
            PError(fmt::format("Refusing to import {}: {} already exists but isn't loaded.",
                import.SourcePaths.front().string(), targetFilePath.string()));

            import.Action = Pine::AssetImportAction::Conflict;
            import.ImportStatus = Pine::AssetImportStatus::Failed;
            return;
        }

        const auto asset = Pine::Assets::Internal::CreateAssetByFile(import.SourcePaths.front());
        if (!asset)
        {
            import.Action = Pine::AssetImportAction::Unsupported;
            import.ImportStatus = Pine::AssetImportStatus::Failed;
            return;
        }

        // Gives the asset its UId and paths. Nothing is written to disk until Execute().
        asset->SetupNew(resolvedEnginePath);

        import.Action = Pine::AssetImportAction::Create;
        import.AssetPtr = asset;
        import.OwnsAsset = true;
    }

    void ProposeImportSettings(AssetImport& import)
    {
        if (import.SettingsProposed || !import.AssetPtr)
        {
            return;
        }

        import.SettingsProposed = true;

        import.AssetPtr->ResolveImportSettings(import);
    }

    // Points the asset at the files it is built from, copying them into the project's content
    // directory first if the context asks for it.
    void SetupSourceFiles(const ImportContext* context, const AssetImport& import)
    {
        const auto asset = import.AssetPtr;

        // On a re-import the asset already carries the sources of its previous import.
        asset->ClearSources();

        for (const auto& sourceFile : import.SourcePaths)
        {
            if (context->CopySourceFiles)
            {
                auto contentFilePath = GenerateContentPath(context, import.EnginePath);

                if (!std::filesystem::exists(context->ContentPath))
                {
                    std::filesystem::create_directories(context->ContentPath);
                }

                // Remove previous content file, if it exists.
                if (std::filesystem::exists(contentFilePath))
                {
                    std::filesystem::remove(contentFilePath);
                }

                // Copy the source file into the projects content directory
                std::filesystem::copy(sourceFile, contentFilePath);

                asset->AddSource(contentFilePath);

                continue;
            }

            asset->AddSource(sourceFile.string());
        }
    }

    void Execute(const ImportContext* context, AssetImport& import)
    {
        if (import.ImportStatus != Pine::AssetImportStatus::Waiting)
        {
            return;
        }

        if (import.Action != Pine::AssetImportAction::Create && import.Action != Pine::AssetImportAction::Update)
        {
            return;
        }

        import.ImportStatus = Pine::AssetImportStatus::Importing;

        // No-op if the caller already did this to show the user what it would decide.
        ProposeImportSettings(import);

        const auto asset = import.AssetPtr;

        // Make sure the directories containing the new asset exists. This was previously skipped
        // for standalone imports, which made writing the '.passet' fail silently when importing
        // into a directory that didn't exist yet.
        const auto assetDirectory = std::filesystem::path(import.ResolvedEnginePath).parent_path();

        if (!assetDirectory.empty() && !std::filesystem::exists(assetDirectory))
        {
            std::filesystem::create_directories(assetDirectory);
        }

        SetupSourceFiles(context, import);

        PInfo(fmt::format("{} {} from source file {}...",
            import.Action == Pine::AssetImportAction::Update ? "Re-importing" : "Importing",
            AssetTypeToString(asset->GetType()),
            import.SourcePaths.front().string()));

        if (!asset->Import(&import))
        {
            PError(fmt::format("Failed to import asset {}", import.SourcePaths.front().string()));

            import.ImportStatus = Pine::AssetImportStatus::Failed;

            if (import.OwnsAsset)
            {
                delete asset;

                import.AssetPtr = nullptr;
                import.OwnsAsset = false;
            }

            return;
        }

        if (!context->DontLoad)
        {
            asset->ReLoad();

            if (import.Action == Pine::AssetImportAction::Create)
            {
                Pine::Assets::Internal::RegisterAsset(asset);
            }
        }
        else
        {
            PInfo(fmt::format("Writing to {}", asset->GetFilePath().string()));
            asset->SaveToFile();
        }

        // Handed off - to the asset manager, or to the caller for a standalone (DontLoad) import.
        import.OwnsAsset = false;

        import.ImportStatus = Pine::AssetImportStatus::Imported;
    }
}

ImportContext* Pine::Importer::CreateContext()
{
    return new ImportContext();
}

void Pine::Importer::DeleteContext(ImportContext* context)
{
    // Anything still owned here was resolved but never successfully imported, e.g. because the
    // caller looked at the plan and dropped it.
    for (const auto& import : context->Imports)
    {
        if (import->OwnsAsset)
        {
            delete import->AssetPtr;

            import->AssetPtr = nullptr;
            import->OwnsAsset = false;
        }
    }

    delete context;
}

void Pine::Importer::AddFile(
    ImportContext* context,
    const std::filesystem::path& sourcePath,
    const std::string& enginePath)
{
    AddFiles(context, {sourcePath}, enginePath);
}

void Pine::Importer::AddFiles(
    ImportContext* context,
    const std::vector<std::filesystem::path>& sourcePaths,
    const std::string& enginePath)
{
    if (sourcePaths.empty())
    {
        return;
    }

    auto import = std::make_unique<AssetImport>();

    import->SourcePaths = sourcePaths;
    import->EnginePath = enginePath;
    import->Context = context;

    // Somewhat hacky but needs to be done to avoid file name duplication.
    for (const auto& iter : context->Imports)
    {
        if (std::filesystem::path(iter->EnginePath).replace_extension("").string() ==
            std::filesystem::path(enginePath).replace_extension("").string())
        {
            import->AvoidDuplicate = true;
            break;
        }
    }

    context->Imports.push_back(std::move(import));
}

void Pine::Importer::Resolve(ImportContext* context)
{
    // Intentionally running .size() based loop here
    for (int i = 0; i < context->Imports.size(); i++)
    {
        ::Resolve(context, *context->Imports[i]);
    }
}

void Pine::Importer::ProposeImportSettings(ImportContext* context)
{
    for (const auto& import : context->Imports)
    {
        ::ProposeImportSettings(*import);
    }
}

void Pine::Importer::Execute(ImportContext* context)
{
    while (ExecuteNext(context))
    {
    }
}

bool Pine::Importer::ExecuteNext(ImportContext* context)
{
    // The queue grows underneath us - importing a model appends the textures it discovers - so
    // the end is re-read every step rather than cached.
    if (context->ExecuteCursor >= context->Imports.size())
    {
        return false;
    }

    // Held by pointer, so appending to the queue below can't invalidate this reference.
    auto& import = *context->Imports[context->ExecuteCursor];

    ::Resolve(context, import);
    ::Execute(context, import);

    context->ExecuteCursor++;

    return true;
}

void Pine::Importer::Run(ImportContext* context)
{
    Resolve(context);
    Execute(context);
}

Pine::Asset* Pine::Importer::ImportRelative(
    const AssetImport* assetImport,
    const std::string& filePath,
    const std::string& overrideFileName,
    const std::function<bool(Asset*)>& configure)
{
    const auto context = assetImport->Context;

    // TODO: This needs to be dynamic depending on what's being imported,
    // currently we only do this for textures so it isn't a problem, yet.
    static constexpr auto subDirectory = "Textures/";

    // An embedded texture has already been written out somewhere by the caller, and carries the
    // name it should be imported under separately. Anything else is a path relative to the asset
    // we're importing the dependency for.
    const auto sourcePath = overrideFileName.empty() ?
        assetImport->SourcePaths.front().parent_path().string() + "/" + filePath : filePath;

    if (overrideFileName.empty() && !std::filesystem::is_regular_file(sourcePath))
    {
        PWarning(fmt::format("Ignoring relative file: {}, could not find it.", filePath));
        return nullptr;
    }

    const auto enginePath = std::filesystem::path(assetImport->EnginePath).parent_path().string() + "/" + subDirectory +
        std::filesystem::path(overrideFileName.empty() ? filePath : overrideFileName).filename().string();

    // Check if this dependency is already in the context queue, either because another asset
    // pulled in the same file, or because it was queued for import on its own.
    AssetImport* queuedImport = nullptr;

    for (const auto& iter : context->Imports)
    {
        const bool sameSource = !iter->SourcePaths.empty() &&
            File::UniversalPath(iter->SourcePaths.front().string()) == File::UniversalPath(sourcePath);
        const bool sameDestination =
            std::filesystem::path(iter->EnginePath).replace_extension("").string() ==
            std::filesystem::path(enginePath).replace_extension("").string();

        if (sameSource || sameDestination)
        {
            queuedImport = iter.get();
            break;
        }
    }

    // Dependencies are imported to their own path and are not copied into the content directory
    // along with whatever pulled them in.
    const bool prevCopySourceFiles = context->CopySourceFiles;

    context->CopySourceFiles = false;

    if (queuedImport == nullptr)
    {
        auto import = std::make_unique<AssetImport>();

        import->SourcePaths = {sourcePath};
        import->EnginePath = enginePath;
        import->Context = context;

        queuedImport = import.get();

        context->Imports.push_back(std::move(import));
    }

    // A top-level texture may have compiled before the model supplies stronger evidence.
    // Rebuild from its established sources, preserving any project content copy.
    if (queuedImport->ImportStatus == AssetImportStatus::Imported &&
        configure && queuedImport->AssetPtr && configure(queuedImport->AssetPtr))
    {
        if (queuedImport->AssetPtr->Import(queuedImport))
        {
            if (context->DontLoad)
                queuedImport->AssetPtr->SaveToFile();
            else
                queuedImport->AssetPtr->ReLoad();
        }
        else
        {
            queuedImport->ImportStatus = AssetImportStatus::Failed;
            PError(fmt::format("Failed to rebuild dependency {}", sourcePath));
        }
    }

    else if (queuedImport->ImportStatus != AssetImportStatus::Imported)
    {
        ::Resolve(context, *queuedImport);
        ::ProposeImportSettings(*queuedImport);

        // Tell the asset what it's for. This is the strongest evidence there is - the file we're
        // importing said so outright - but it still goes in through the asset's own settings,
        // which is what keeps it from overwriting a choice made by hand on a re-import.
        if (configure && queuedImport->AssetPtr)
        {
            configure(queuedImport->AssetPtr);
        }

        ::Execute(context, *queuedImport);
    }

    context->CopySourceFiles = prevCopySourceFiles;

    return queuedImport->ImportStatus == AssetImportStatus::Imported ? queuedImport->AssetPtr : nullptr;
}

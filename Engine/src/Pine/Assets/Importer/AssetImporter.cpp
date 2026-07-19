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

    void Import(
        const ImportContext* context,
        AssetImport& import)
    {
        if (import.ImportStatus != Pine::AssetImportStatus::Waiting || import.SourcePaths.empty())
        {
            return;
        }

        import.ImportStatus = Pine::AssetImportStatus::Importing;

        const auto asset = Pine::Assets::Internal::CreateAssetByFile(import.SourcePaths.front());
        if (!asset)
        {
            import.ImportStatus = Pine::AssetImportStatus::Failed;
            return;
        }

        // Make sure the directories containing the new asset exists
        if (!context->DontLoad)
        {
            if (!std::filesystem::exists(std::filesystem::path(import.EnginePath).parent_path()))
            {
                std::filesystem::create_directories(std::filesystem::path(import.EnginePath).parent_path());
            }
        }

        auto suggestedEnginePath = std::filesystem::path(import.EnginePath).replace_extension("").string();

        if (import.AvoidDuplicate)
        {
            suggestedEnginePath += Pine::String::Replace(import.SourcePaths.front().extension().string(), ".", "-");
        }

        asset->SetupNew(suggestedEnginePath);

        for (const auto& sourceFile : import.SourcePaths)
        {
            if (context->CopySourceFiles)
            {
                auto contentFilePath = GenerateContentPath(context, import.EnginePath);

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

        PInfo(fmt::format("Importing {} from source file {}...", AssetTypeToString(asset->GetType()), import.SourcePaths.front().string()));

        if (!asset->Import(&import))
        {
            PError(fmt::format("Failed to import asset {}", import.SourcePaths.front().string()));

            import.ImportStatus = Pine::AssetImportStatus::Failed;
            delete asset;
            return;
        }

        if (!context->DontLoad)
        {
            asset->ReLoad();
            Pine::Assets::Internal::RegisterAsset(asset);
        }
        else
        {
            PInfo(fmt::format("Writing to {}", asset->GetFilePath().string()));
            asset->SaveToFile();
        }

        import.ImportStatus = Pine::AssetImportStatus::Imported;
        import.AssetPtr = asset;
    }
}

ImportContext* Pine::Importer::CreateContext()
{
    auto ret = new ImportContext();

    // Fix.

    return ret;
}

void Pine::Importer::DeleteContext(const ImportContext* context)
{
    delete context;
}

void Pine::Importer::AddFile(
    ImportContext* context,
    const std::filesystem::path& sourcePath,
    const std::string& enginePath,
    AssetImportConfiguration* configuration)
{
    AssetImport import;

    import.Configuration = configuration;
    import.SourcePaths = {sourcePath};
    import.EnginePath = enginePath;
    import.Context = context;

    // Somewhat hacky but needs to be done to avoid file name duplication.
    for (const auto& iter : context->Imports)
    {
        if (std::filesystem::path(iter.EnginePath).replace_extension("").string() ==
            std::filesystem::path(enginePath).replace_extension("").string())
        {
            import.AvoidDuplicate = true;
            break;
        }
    }

    context->Imports.push_back(import);
}

void Pine::Importer::Run(ImportContext* context)
{
    // Intentionally running .size() based loop here
    for (int i = 0; i < context->Imports.size(); i++)
    {
        Import(context, context->Imports[i]);
    }
}

Pine::Asset* Pine::Importer::ImportRelative(
    const AssetImport* assetImport,
    const std::string& filePath,
    const std::string& overrideFileName)
{
    const auto relativeFilePath = assetImport->SourcePaths.front().parent_path().string();

    if (overrideFileName.empty())
    {
        if (!std::filesystem::is_regular_file(relativeFilePath + "/" + filePath))
        {
            PWarning(fmt::format("Ignoring relative file: {}, could not find it.", filePath));
            return nullptr;
        }
    }

    // Check if this asset is already in the context queue
    for (auto& iter : assetImport->Context->Imports)
    {
        if (!String::EndsWith(File::UniversalPath(iter.SourcePaths.front().string()), File::UniversalPath(relativeFilePath + "/" + filePath)))
        {
            continue;
        }

        // Might have already been imported previously, we can just return the imported asset
        if (iter.ImportStatus == AssetImportStatus::Imported)
        {
            return iter.AssetPtr;
        }

        // Otherwise try to import
        Import(assetImport->Context, iter);

        if (iter.ImportStatus == AssetImportStatus::Imported)
        {
            return iter.AssetPtr;
        }

        return nullptr;
    }

    AssetImport import;

    import.Configuration = nullptr;
    import.SourcePaths = {filePath};
    import.Context = assetImport->Context;

    import.EnginePath = std::filesystem::path(assetImport->EnginePath).parent_path().string() + "/Textures/" +
        std::filesystem::path(overrideFileName.empty() ? filePath : overrideFileName).filename().string();

    const bool prevCopySourceFiles = assetImport->Context->CopySourceFiles;

    assetImport->Context->CopySourceFiles = false;

    Import(assetImport->Context, import);

    assetImport->Context->CopySourceFiles = prevCopySourceFiles;

    if (import.ImportStatus == AssetImportStatus::Imported)
    {
        return import.AssetPtr;
    }

    return nullptr;
}
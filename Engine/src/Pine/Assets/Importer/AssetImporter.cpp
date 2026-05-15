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
        if (!std::filesystem::exists(std::filesystem::path(import.EnginePath).parent_path()))
        {
            std::filesystem::create_directories(std::filesystem::path(import.EnginePath).parent_path());
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
            import.ImportStatus = Pine::AssetImportStatus::Failed;
            delete asset;
            return;
        }

        asset->ReLoad();

        Pine::Assets::Internal::RegisterAsset(asset);

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
    for (auto& import : context->Imports)
    {
        Import(context, import);
    }
}

Pine::Asset* Pine::Importer::ImportRelative(
    const AssetImport* assetImport,
    const std::string& filePath)
{
    const auto relativeFilePath = assetImport->SourcePaths.front().parent_path().string();

    if (!std::filesystem::is_regular_file(relativeFilePath + "/" + filePath))
    {
        return nullptr;
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

    // TODO: Import as a new file?

    return nullptr;
}
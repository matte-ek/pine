#include <iostream>
#include <set>
#include <nlohmann/json.hpp>
#include <Pine/Pine.hpp>
#include <Pine/Core/Serialization/Json/SerializationJson.hpp>

#include "Pine/Assets/Importer/AssetImporter.hpp"
#include "Pine/Assets/Material/Material.hpp"
#include "Pine/Assets/Shader/Shader.hpp"
#include "Pine/Core/File/File.hpp"
#include "Pine/Core/String/String.hpp"

namespace
{
    // Imports one asset (built from one or more source files) and writes its .passet to
    // <enginePath>.passet. Runs "standalone" (DontLoad) so nothing is loaded/compiled --
    // this keeps the CLI headless (no graphics context required). Returns the imported
    // asset (still owned by the caller) or nullptr on failure.
    Pine::Asset* ImportAsset(const std::vector<std::filesystem::path>& sourceFiles, const std::string& enginePath)
    {
        auto context = Pine::Importer::CreateContext();
        context->DontLoad = true;

        Pine::Importer::AddFiles(context, sourceFiles, enginePath);
        Pine::Importer::Run(context);

        Pine::Asset* asset = nullptr;
        if (!context->Imports.empty() && context->Imports.front()->ImportStatus == Pine::AssetImportStatus::Imported)
        {
            asset = context->Imports.front()->AssetPtr;
        }

        // DeleteContext only frees the context, not the imported asset it points to.
        Pine::Importer::DeleteContext(context);

        return asset;
    }
}

int main(int argc, const char* argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage:" << std::endl;
        std::cout << "  EngineCli --import <output> <input file>..." << std::endl;
        std::cout << "  EngineCli --batch-import <directory> [<map-root>]" << std::endl;
        return 1;
    }

    if (strcmp(argv[1], "--batch-import") == 0)
    {
        if (argc < 3 || !std::filesystem::is_directory(argv[2]))
        {
            std::cout << "Usage: EngineCli --batch-import <directory> [<map-root>]" << std::endl;
            return 1;
        }

        bool mapAsRoot = argc == 4;

        static std::set<std::string> batchSupportedFileExtensions = {
            ".png",
            ".jpeg",
            ".jpg",
            ".fbx",
            ".glb",
            ".dae",
            ".ih"
        };

        std::unordered_map<std::string, Pine::Asset*> assetSourceFileLookupMap;

        // First step is figuring out which assets have been imported before, so we can override
        // that data (if updated), and not create a new asset of an already imported asset.
        for (const auto& iter : std::filesystem::directory_iterator("data"))
        {
            if (iter.is_directory() || iter.path().extension().string() != ".passet")
            {
                continue;
            }

            auto asset = Pine::Asset::LoadFromFile(iter.path(), true);
            if (!asset)
            {
                continue;
            }

            if (asset->GetSources().empty())
            {
                delete asset;
                continue;
            }

            for (const auto& sourceFile : asset->GetSources())
            {
                assetSourceFileLookupMap[sourceFile.FilePath] = asset;
            }
        }

        // Next we can go digging inside the specified load directory.
        for (const auto& iter : std::filesystem::recursive_directory_iterator(argv[2]))
        {
            if (iter.is_directory())
            {
                continue;
            }

            // Has the file has already been imported before?
            if (assetSourceFileLookupMap.count(iter.path().string()) > 0)
            {
                auto asset = assetSourceFileLookupMap[iter.path().string()];

                for (const auto& assetSource : asset->GetSources())
                {
                    if (assetSource.FilePath != Pine::File::UniversalPath(iter.path().string()))
                    {
                        continue;
                    }

                    if (std::filesystem::last_write_time(iter).time_since_epoch().count() != assetSource.LastWriteTime)
                    {
                        std::cout << "Reloading file " << iter.path() << " due to file being updated." << std::endl;

                        asset->Import();
                        asset->SaveToFile();

                        break;
                    }
                }

                continue;
            }

            auto targetFile = iter.path();
            auto extension = Pine::String::ToLower(targetFile.extension().string());
            auto mappedPath = std::filesystem::path(targetFile).replace_extension("").string();

            if (mapAsRoot)
            {
                mappedPath = mappedPath.substr(strlen(argv[2]));
            }

            if (batchSupportedFileExtensions.count(extension) == 0)
            {
                continue;
            }

            std::vector<std::filesystem::path> sourceFiles;
            nlohmann::json importHint;
            const bool isImportHint = extension == ".ih";

            // For "import hint" files, we get some extra information over what to do.
            if (isImportHint)
            {
                importHint = Pine::SerializationJson::LoadFromFile(targetFile).value();

                bool ignoreImportHint = false;

                for (const auto& sourceFile : importHint["SourceFiles"])
                {
                    auto sourceFileFullPath = targetFile.parent_path().string() + "/" + sourceFile.get<std::string>();

                    if (assetSourceFileLookupMap.count(sourceFileFullPath) != 0)
                    {
                        ignoreImportHint = true;
                        break;
                    }

                    sourceFiles.emplace_back(sourceFileFullPath);
                }

                if (ignoreImportHint)
                {
                    continue;
                }
            }
            else
            {
                sourceFiles.emplace_back(targetFile);
            }

            auto asset = ImportAsset(sourceFiles, mappedPath);

            if (!asset)
            {
                std::cerr << "Failed to import asset: " << targetFile << std::endl;
                continue;
            }

            // Process shader "custom" data provided by the import hint.
            if (isImportHint && asset->GetType() == Pine::AssetType::Shader)
            {
                auto shader = dynamic_cast<Pine::Shader*>(asset);

                assert(shader);

                for (const auto& textureSampler : importHint["Data"]["TextureSamplers"].items())
                {
                    shader->AddTextureSamplerBinding(textureSampler.key(), textureSampler.value());
                }

                for (const auto& version : importHint["Data"]["Versions"].items())
                {
                    shader->AddVersion(version.key(), version.value());
                }

                // Persist the extra data added on top of the imported source.
                asset->SaveToFile();
            }

            // Pine itself doesn't really care that much about the assets folder after the file has been
            // imported, but the user might care. Therefore, create a hard link to the location of the source
            // file, to make it (maybe) clearer to the user (and editor).
            auto linkPath = std::filesystem::path(targetFile).replace_extension(".passet");

            if (std::filesystem::exists(linkPath))
            {
                std::filesystem::remove(linkPath);
            }

            if (Pine::File::UniversalPath(linkPath.string()) != Pine::File::UniversalPath(asset->GetFilePath().string()))
            {
                std::filesystem::create_hard_link(asset->GetFilePath(), linkPath);
            }

            std::cout << "Imported file " << targetFile << " as " << asset->GetUId().ToString() << std::endl;

            delete asset;
        }

        return 0;
    }

    if (strcmp(argv[1], "--import") == 0)
    {
        // Import single asset
        if (argc < 4)
        {
            std::cout << "Usage: EngineCli --import <output> <input file>..." << std::endl;
            return 1;
        }

        const auto enginePath = std::filesystem::path(argv[2]).replace_extension("").string();
        const auto sourceFilesCount = argc - 3;

        std::vector<std::filesystem::path> sourceFiles;
        for (size_t i{}; i < sourceFilesCount; i++)
        {
            sourceFiles.emplace_back(argv[3 + i]);
        }

        auto asset = ImportAsset(sourceFiles, enginePath);

        if (!asset)
        {
            std::cerr << "Failed to import asset." << std::endl;
            return 1;
        }

        std::cout << "Imported " << asset->GetPath() << " -> " << asset->GetFilePath().string()
                  << " (" << asset->GetUId().ToString() << ")" << std::endl;

        delete asset;

        return 0;
    }

    std::cerr << "Unknown command: " << argv[1] << std::endl;
    return 1;
}

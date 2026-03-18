#include <iostream>
#include <set>
#include <Pine/Pine.hpp>
#include <Pine/Core/Serialization/Json/SerializationJson.hpp>

#include "Pine/Assets/Material/Material.hpp"
#include "Pine/Assets/Shader/Shader.hpp"
#include "Pine/Core/File/File.hpp"
#include "Pine/Core/String/String.hpp"

namespace
{

}

int main(int argc, const char* argv[])
{
    if (argc < 2)
    {
        std::cout << "Use `EngineCli help` for list of commands." << std::endl;
        return 1;
    }

    if (strcmp(argv[1], "--batch-import") == 0)
    {
        if (argc < 3 || !std::filesystem::is_directory(argv[2]))
        {
            std::cout << "Usage: EngineCli --batch-import <directory> <map-root>" << std::endl;
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

            // For "import hint" files, we get some extra information over what to do.
            if (extension == ".ih")
            {
                auto j = Pine::SerializationJson::LoadFromFile(targetFile).value();

                bool ignoreImportHint = false;
                std::vector<std::filesystem::path> sourceFiles;

                for (const auto& sourceFile : j["SourceFiles"])
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

                auto asset = Pine::Assets::ImportAssetFromFiles(
                    sourceFiles,
                    mappedPath);

                if (!asset)
                {
                    std::cerr << "Failed to import asset: " << targetFile << std::endl;
                    continue;
                }

                // Process shader "custom" data
                if (asset->GetType() == Pine::AssetType::Shader)
                {
                    auto shader = dynamic_cast<Pine::Shader*>(asset);

                    assert(shader);

                    for (const auto& textureSampler : j["Data"]["TextureSamplers"].items())
                    {
                        shader->AddTextureSamplerBinding(textureSampler.key(), textureSampler.value());
                    }

                    for (const auto& textureSampler : j["Data"]["Versions"].items())
                    {
                        shader->AddVersion(textureSampler.key(), textureSampler.value());
                    }
                }

                asset->SaveToFile();

                if (std::filesystem::exists(targetFile.replace_extension(".passet")))
                {
                    std::filesystem::remove(targetFile.replace_extension(".passet"));
                }

                // Pine itself doesn't really care that much about the assets folder after the file has been
                // imported, but the user might care. Therefore, create a hard link to the location of the source
                // file, to make it (maybe) clearer to the user (and editor).
                std::filesystem::create_hard_link(asset->GetFilePath(), targetFile.replace_extension(".passet"));

                std::cout << "Imported file " << targetFile << " as " << asset->GetUId().ToString() << std::endl;

                delete asset;
            }
            else
            {
                auto asset = Pine::Assets::ImportAssetFromFile(
                    targetFile,
                    mappedPath);

                if (!asset)
                {
                    std::cerr << "Failed to import asset: " << targetFile << std::endl;
                    return 1;
                }

                asset->SaveToFile();

                if (std::filesystem::exists(targetFile.replace_extension(".passet")))
                {
                    std::filesystem::remove(targetFile.replace_extension(".passet"));
                }

                std::filesystem::create_hard_link(asset->GetFilePath(), targetFile.replace_extension(".passet"));

                std::cout << "Imported file " << targetFile << " as " << asset->GetUId().ToString() << std::endl;

                delete asset;
            }
        }

        return 0;
    }

    if (strcmp(argv[1], "--import") == 0)
    {
        // Import single asset
        if (argc < 4)
        {
            std::cout << "Usage: EngineCli --import <output> <input file> ..." << std::endl;
            return 1;
        }

        const auto targetFile = argv[2];
        const auto sourceFilesCount = argc - 3;

        std::vector<std::filesystem::path> sourceFiles;
        for (size_t i{}; i < sourceFilesCount; i++)
        {
            sourceFiles.emplace_back(argv[3 + i]);
        }

        auto asset = Pine::Assets::ImportAssetFromFiles(
            sourceFiles,
            std::filesystem::path(targetFile).replace_extension("").string());

        if (!asset)
        {
            std::cerr << "Failed to import asset." << std::endl;
            return 1;
        }

        asset->SaveToFile();

        if (std::filesystem::exists(std::filesystem::path(targetFile).replace_extension(".passet")))
        {
            std::filesystem::remove(std::filesystem::path(targetFile).replace_extension(".passet"));
        }

        std::filesystem::create_hard_link(asset->GetFilePath(), std::filesystem::path(targetFile).replace_extension(".passet"));

        delete asset;
    }

    return 0;
}
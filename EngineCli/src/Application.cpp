#include <iostream>
#include <Pine/Pine.hpp>
#include <Pine/Core/Serialization/Json/SerializationJson.hpp>

#include "Pine/Assets/Material/Material.hpp"
#include "Pine/Assets/Shader/Shader.hpp"

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

    if (strcmp(argv[1], "--import") == 0)
    {
        // Import directory
        if (argc == 3)
        {
            std::filesystem::path targetDirectory = argv[2];

            if (!(std::filesystem::exists(targetDirectory) && std::filesystem::is_directory(targetDirectory)))
            {
                std::cout << "Target not directory." << std::endl;
                return 1;
            }

            for (const auto& entry : std::filesystem::recursive_directory_iterator(targetDirectory))
            {
                auto targetFile = entry.path();

                if (targetFile.extension() != ".ih")
                {
                    continue;
                }

                std::cout << "Processing " << targetFile << "..." << std::endl;

                auto j = Pine::SerializationJson::LoadFromFile(targetFile).value();

                std::vector<std::filesystem::path> sourceFiles;

                for (const auto& sourceFile : j["SourceFiles"])
                {
                    sourceFiles.emplace_back(targetFile.parent_path().string() + "/" + sourceFile.get<std::string>());
                }

                auto asset = Pine::Assets::ImportAssetFromFiles(
                    sourceFiles,
                    targetFile.replace_extension("").string(),
                    targetFile.replace_extension(".passet").string());

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

                delete asset;
            }

            return 0;
        }

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
            std::filesystem::path(targetFile).replace_extension("").string(),
            targetFile);

        if (!asset)
        {
            std::cerr << "Failed to import asset." << std::endl;
            return 1;
        }

        asset->SaveToFile();
    }

    return 0;
}
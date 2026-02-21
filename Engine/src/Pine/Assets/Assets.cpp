#include "Assets.hpp"

#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Assets/Material/Material.hpp"
#include "Pine/Assets/Blueprint/Blueprint.hpp"
#include "Pine/Assets/Font/Font.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/Assets/Shader/Shader.hpp"
#include "Pine/Assets/Texture2D/Texture2D.hpp"
#include "Pine/Assets/Tilemap/Tilemap.hpp"
#include "Pine/Assets/Tileset/Tileset.hpp"
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Core/String/String.hpp"
#include "Pine/Assets/Texture3D/Texture3D.hpp"
#include "Pine/Assets/AudioFile/AudioFile.hpp"
#include "Pine/Assets/CSharpScript/CSharpScript.hpp"

#include <functional>
#include <thread>
#include <unordered_map>
#include <mutex>

#include "Pine/Threading/Threading.hpp"

using namespace Pine;

namespace
{
    AssetManagerState m_State;

    // All assets currently registered by Pine, they don't have to be
    // valid assets, or loaded assets.
    std::unordered_map<Guid, Asset*> m_AssetsMapGuid;
    std::unordered_map<std::string, Asset*> m_AssetsMapPath;
    std::unordered_map<std::string, Asset*> m_AssetsMapFilePath;

    std::mutex m_AssetsMutex;

    std::string m_WorkingDirectory;

    struct AssetImportFactory
    {
        // The file extension(s) for this asset type
        std::vector<std::string> m_FileExtensions;

        AssetType m_Type;

        // This function should create the asset object itself
        std::function<Asset*()> m_Factory;
    };

    std::vector m_AssetImportFactories =
    {
        AssetImportFactory( { { "png", "jpg", "jpeg", "tga", "bmp", "gif" }, AssetType::Texture2D, [](){ return new Texture2D(); } } ),
        AssetImportFactory( { { "cmap" }, AssetType::Texture3D, [](){ return new Texture3D(); } } ),
        AssetImportFactory( { { "obj", "fbx", "glb", "dae", "gltf" }, AssetType::Model, [](){ return new Model(); } } ),
        AssetImportFactory( { { "mat" }, AssetType::Material, [](){ return new Material(); } } ),
        AssetImportFactory( { { "ttf" }, AssetType::Font, [](){ return new Font(); } } ),
        AssetImportFactory( { { "glsl" }, AssetType::Shader, [](){ return new Shader(); } } ),
        AssetImportFactory( { { "bpt" }, AssetType::Blueprint, [](){ return new Blueprint(); } } ),
        AssetImportFactory( { { "lvl" }, AssetType::Level, [](){ return new Level(); } } ),
        AssetImportFactory( { { "tset" }, AssetType::Tileset, [](){ return new Tileset(); } } ),
        AssetImportFactory( { { "tmap" }, AssetType::Tilemap, [](){ return new Tilemap(); } } ),
        AssetImportFactory( { { "wav", "wave", "flac", "ogg", "oga", "spx" }, AssetType::Audio, [](){ return new AudioFile(); } } ),
        AssetImportFactory( { { "cs" }, AssetType::CSharpScript, [](){ return new CSharpScript(); } } )
    };

    // Attempts to find an asset factory with the file name extension (can be full path as well)
    AssetImportFactory const* GetAssetFactoryFromFileName(const std::filesystem::path& fileName)
    {
        if (!fileName.has_extension())
            return nullptr;

        const auto inputFileExtension = fileName.extension().string().substr(1);

        for (const auto& factory : m_AssetImportFactories)
        {
            for (const auto& extension : factory.m_FileExtensions)
            {
                if (inputFileExtension == extension)
                {
                    return &factory;
                }
            }
        }

        return nullptr;
    }

    std::shared_ptr<Task> RunAssetLoadFromFile(const std::filesystem::path& filePath, TaskPool* taskPool = nullptr)
    {
        if (filePath.extension().string() != ".passet")
        {
            return nullptr;
        }

        return Threading::QueueTask<Asset*>([filePath]() -> Asset*
        {
            if (!std::filesystem::exists(filePath))
            {
                return nullptr;
            }

            auto data = File::ReadCompressed(filePath);
            auto asset = Asset::Load(data, filePath.string());

            if (asset)
            {
                std::unique_lock lock(m_AssetsMutex);

                m_AssetsMapPath[asset->GetPath()] = asset;
                m_AssetsMapGuid[asset->GetGuid()] = asset;
                m_AssetsMapFilePath[asset->GetFilePath().string()] = asset;

                lock.unlock();

                Pine::Log::Verbose(fmt::format("Loaded asset {} as {} successfully.", asset->GetPath(), AssetTypeToString(asset->GetType())));
            }
            else
            {
                Pine::Log::Error(fmt::format("Failed to load asset {} as {}.", asset->GetPath(), AssetTypeToString(asset->GetType())));
            }

            return asset;
        },
        TaskThreadingMode::Default,
        taskPool);
    }
}

void Assets::Setup()
{
}

void Assets::Shutdown()
{
    for (auto& [path, asset] : m_AssetsMapGuid)
    {
        Log::Verbose(fmt::format("Disposing asset {}...", path.ToString()));
        asset->Dispose();
    }
}

void Assets::SetWorkingDirectory(std::string_view workingDirectory)
{
    m_WorkingDirectory = workingDirectory;
}

Asset* Assets::LoadAssetFromFile(const std::filesystem::path& filePath)
{
    auto task = RunAssetLoadFromFile(m_WorkingDirectory + filePath.string());

    if (task == nullptr)
    {
        return nullptr;
    }

    return static_cast<Asset*>(Threading::AwaitTaskResult(task));
}

int Assets::LoadAssetsFromDirectory(const std::filesystem::path& directory)
{
    int loadedAssets = 0;
    std::vector<std::shared_ptr<Task>> assetLoadTasks;

    // Get the correct path with the possible working directory and make sure it's valid.
    std::filesystem::path finalPath = m_WorkingDirectory + directory.string();
    if (!std::filesystem::exists(finalPath))
    {
        return -1;
    }

    auto pool = Threading::CreateTaskPool(true);

    // Iterate through the specified directory
    for (const auto& iter : std::filesystem::recursive_directory_iterator(finalPath))
    {
        // Make sure it's a pine asset file
        if (iter.is_directory() || iter.path().extension().string() != ".passet")
        {
            continue;
        }

        if (m_AssetsMapFilePath.count(File::UniversalPath(iter.path().string())) != 0)
        {
            continue;
        }

        assetLoadTasks.push_back(RunAssetLoadFromFile(iter, pool));
    }

    // Wait for all the tasks to complete
    Threading::AwaitTaskPool(pool);

    // Count the amount of loaded assets
    for (const auto& task : assetLoadTasks)
    {
        if (task->Result != nullptr)
        {
            loadedAssets++;
        }
        else
        {
            loadedAssets = -1;
            break;
        }
    }

    delete pool;

    return loadedAssets;
}

Asset* Assets::ImportAssetFromFile(const std::filesystem::path& sourceFilePath, std::string_view outputFilePath)
{
    // Get the correct path with the possible working directory and make sure it's valid.
    std::filesystem::path finalPath = m_WorkingDirectory + sourceFilePath.string();
    if (!std::filesystem::exists(finalPath))
    {
        return nullptr;
    }

    std::string outputFile = outputFilePath.empty() ? finalPath.replace_extension(".passet").string() : std::string(outputFilePath.data());

    return ImportAssetFromFiles({sourceFilePath}, outputFile, outputFilePath);
}

Asset* Assets::ImportAssetFromFiles(const std::vector<std::filesystem::path>& sourceFilePaths, std::string_view mappedPath, std::string_view outputFilePath)
{
    if (sourceFilePaths.empty())
    {
        return nullptr;
    }

    // Use at least one source file to figure out the type.
    auto factory = GetAssetFactoryFromFileName(sourceFilePaths.front());
    if (!factory)
    {
        return nullptr;
    }

    auto asset = factory->m_Factory();

    asset->SetupNew(mappedPath.data(), outputFilePath);

    for (const auto& source : sourceFilePaths)
    {
        asset->AddSource(source.string());
    }

    if (!asset->Import())
    {
        delete asset;
        return nullptr;
    }

    return asset;
}

Asset* Assets::CreateAsset(AssetType type, std::string_view assetPath)
{
    auto asset = Internal::CreateAssetByType(type);
    if (!asset)
    {
        return nullptr;
    }

    asset->SetupNew(assetPath.data(), "");

    return asset;
}

Asset* Assets::GetAssetByGuid(Guid id)
{
    if (m_AssetsMapGuid.count(id) == 0)
    {
        return nullptr;
    }

    return m_AssetsMapGuid[id];
}

Asset* Assets::GetAssetByPath(std::string_view path)
{
    if (m_AssetsMapPath.count(path.data()) == 0)
    {
        return nullptr;
    }

    return m_AssetsMapPath[path.data()];
}

AssetManagerState Assets::Internal::GetState()
{
    return m_State;
}

Asset* Assets::Internal::CreateAssetByType(AssetType type)
{
    for (const auto& assetFactory : m_AssetImportFactories)
    {
        if (assetFactory.m_Type == type)
        {
            return assetFactory.m_Factory();
        }
    }

    return nullptr;
}
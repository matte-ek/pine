#include "Assets.hpp"

#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Assets/AudioFile/AudioFile.hpp"
#include "Pine/Assets/Blueprint/Blueprint.hpp"
#include "Pine/Assets/CSharpScript/CSharpScript.hpp"
#include "Pine/Assets/Font/Font.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/Assets/Material/Material.hpp"
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/Assets/Shader/Shader.hpp"
#include "Pine/Assets/Texture2D/Texture2D.hpp"
#include "Pine/Assets/Texture3D/Texture3D.hpp"
#include "Pine/Assets/Tilemap/Tilemap.hpp"
#include "Pine/Assets/Tileset/Tileset.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Core/String/String.hpp"

#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "Pine/Threading/Threading.hpp"

using namespace Pine;

namespace
{
    std::string m_WorkingDirectory = "./";

    std::mutex m_AssetsMutex;

    // All assets currently registered by Pine, they don't have to be
    // valid assets, or loaded assets.
    std::unordered_map<UId, Asset*> m_AssetsMapUId;
    std::unordered_map<std::string, Asset*> m_AssetsMapPath;

    // A list of currently loading assets, we use this to determine if
    // an asset is currently already loading during dependencies.
    std::mutex m_LoadingAssetsMutex;
    std::vector<std::pair<UId, std::shared_ptr<Task>>> m_LoadingAssets;

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
            // Is the input valid?
            if (!std::filesystem::exists(filePath))
            {
                return nullptr;
            }

            // Get the id from the file name, this requires all file names to contain the UId
            // which might not exactly be ideal. I am not sure of any problems with this currently,
            // but it might be a thing to look into.
            auto assetId = UId(filePath.filename().string());

            // Make sure this asset is not already being imported, this could happen for example
            // when dealing with dependencies, as the asset being imported might try to load the
            // asset before us.
            std::unique_lock loadingAssetsLock(m_LoadingAssetsMutex);
            for (const auto& loadingAsset : m_LoadingAssets)
            {
                if (loadingAsset.first == assetId)
                {
                    // We're already being imported, exit out.
                    return nullptr;
                }
            }

            // If not, add ourselves to the list.
            m_LoadingAssets.emplace_back(assetId, Threading::GetCurrentTask());
            loadingAssetsLock.unlock();

            // Load the asset itself
            auto data = File::ReadCompressed(filePath);
            auto asset = Asset::Load(data, filePath.string());

            if (asset)
            {
                // Add the imported asset to our maps
                std::unique_lock lock(m_AssetsMutex);
                m_AssetsMapPath[asset->GetPath()] = asset;
                m_AssetsMapUId[asset->GetUId()] = asset;
                lock.unlock();

                // Remove ourselves from the "currently loading assets" list
                loadingAssetsLock.lock();
                for (size_t i{}; i < m_LoadingAssets.size(); i++)
                {
                    if (m_LoadingAssets[i].first == assetId)
                    {
                        m_LoadingAssets.erase(m_LoadingAssets.begin() + i);
                        break;
                    }
                }
                loadingAssetsLock.unlock();

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
    for (auto& [path, asset] : m_AssetsMapUId)
    {
        Log::Verbose(fmt::format("Disposing asset {}...", path.ToString()));
        asset->Dispose();
    }
}

void Assets::SetWorkingDirectory(std::string_view workingDirectory)
{
    m_WorkingDirectory = workingDirectory;

    if (m_WorkingDirectory.empty())
    {
        m_WorkingDirectory = "./";
    }
    else if (m_WorkingDirectory.at(m_WorkingDirectory.size() - 1) != '/')
    {
        m_WorkingDirectory += "/";
    }
}

Asset* Assets::LoadAssetFromFile(const std::filesystem::path& filePath)
{
    auto assetId = UId(filePath.filename().string());

    // Check if we're already loaded this asset.
    std::unique_lock assetLock(m_AssetsMutex);
    if (m_AssetsMapUId.count(assetId))
    {
        return m_AssetsMapUId.at(assetId);
    }
    assetLock.unlock();

    // If not, load it.
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

        // Make sure it's not already been imported.
        {
            std::unique_lock assetLock(m_AssetsMutex);
            if (m_AssetsMapUId.count(UId(iter.path().filename().string())) != 0)
            {
                continue;
            }
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

Asset* Assets::ImportAssetFromFile(const std::filesystem::path& sourceFilePath, std::string_view mappedPath)
{
    // Get the correct path with the possible working directory and make sure it's valid.
    std::filesystem::path finalPath = m_WorkingDirectory + sourceFilePath.string();
    if (!std::filesystem::exists(finalPath))
    {
        return nullptr;
    }

    return ImportAssetFromFiles({sourceFilePath}, mappedPath);
}

Asset* Assets::ImportAssetFromFiles(const std::vector<std::filesystem::path>& sourceFilePaths, std::string_view mappedPath)
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

    asset->SetupNew(mappedPath.data());

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

    asset->SetupNew(assetPath.data());

    return asset;
}

Asset* Assets::GetAssetByUId(UId id)
{
    if (m_AssetsMapUId.count(id) == 0)
    {
        return nullptr;
    }

    return m_AssetsMapUId[id];
}

Asset* Assets::GetAssetByPath(std::string_view path)
{
    if (m_AssetsMapPath.count(path.data()) == 0)
    {
        return nullptr;
    }

    return m_AssetsMapPath[path.data()];
}

const std::unordered_map<UId, Asset*>& Assets::GetAll()
{
    return m_AssetsMapUId;
}

const std::string& Assets::Internal::GetWorkingDirectory()
{
    return m_WorkingDirectory;
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

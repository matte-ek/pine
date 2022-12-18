#include "Assets.hpp"
#include "Pine/Assets/InvalidAsset/InvalidAsset.hpp"
#include "Pine/Assets/Texture2D/Texture2D.hpp"
#include "Pine/Core/String/String.hpp"
#include <cmath>
#include <stdexcept>
#include <thread>
#include <unordered_map>

using namespace Pine;

namespace
{
    // All assets currently registered by Pine, they don't have to be
    // valid assets, or loaded assets.
    std::unordered_map<std::string, IAsset*> m_Assets;

    struct AssetFactory
    {
        // The file extension(s) for this asset type
        std::vector<std::string> m_FileExtensions;

        Pine::AssetType m_Type;

        // This function should create the asset object itself
        std::function<Pine::IAsset*()> m_Factory;
    };

    std::vector<AssetFactory> m_AssetFactories = {
        AssetFactory( { { "png", "jpg", "jpeg", "tga", "bmp", "gif" }, AssetType::Texture2D, [](){ return new Texture2D(); } } )
    };

    // Attempts to find an asset factory with the file name extension (can be full path as well)
    AssetFactory const* GetAssetFactoryFromFileName(const std::filesystem::path& fileName)
    {
        if (!fileName.has_extension())
            return nullptr;

        const auto inputFileExtension = fileName.extension().string().substr(1);

        for (const auto& factory : m_AssetFactories)
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

    // Works out the path string that would be used within the engine.
    std::string GetAssetMapPath(const std::filesystem::path& path, const std::string& rootPath, const std::string& mapPath)
    {
        // Always prefer a manually specified map path
        if (!mapPath.empty())
            return mapPath;

        // If we don't want to 'change' the relative directory,
        // we can just return the file path itself.
        if (rootPath.empty())
            return path.string();

        auto filePath = path.string();

        if (Pine::String::StartsWith(filePath, rootPath))
        {
            return filePath.substr(rootPath.size());
        }

        return filePath;
    }

    Pine::IAsset* PrepareAssetFromFile(const std::filesystem::path& path, const std::string& rootPath, const std::string& mapPath)
    {
        if (!std::filesystem::exists(path))
            return nullptr;

        const auto factory = GetAssetFactoryFromFileName(path.filename());

        IAsset* asset;

        if (factory != nullptr)
        {
            asset = factory->m_Factory();
        }
        else
        {
            asset = new InvalidAsset();
        }

        asset->SetFilePath(path);
        asset->SetPath(GetAssetMapPath(path, rootPath, mapPath));

        return asset;
    }

    enum class LoadThreadingModeContext
    {
        SingleThread,
        MultiThread
    };

    bool LoadAssetDataFromFile(Pine::IAsset* asset, LoadThreadingModeContext threadingModeContext, AssetLoadStage loadStage)
    {
        // Verify that we are in the main thread if the asset's load mode requires us to be
        if ((asset->GetLoadMode() == AssetLoadMode::SingleThread ||
            asset->GetLoadMode() == AssetLoadMode::MultiThreadPrepare) &&
            threadingModeContext == LoadThreadingModeContext::MultiThread)
        {
            if (asset->GetLoadMode() == AssetLoadMode::SingleThread)
            {
                throw std::runtime_error("Wrong thread during asset load.");
            }
        }

        return asset->LoadFromFile(loadStage);
    }
}

void Pine::Assets::Setup()
{
}

void Pine::Assets::Shutdown()
{
    for (auto& asset : m_Assets)
    {
        asset.second->Dispose();
    }
}

Pine::IAsset* Pine::Assets::LoadFromFile(const std::filesystem::path& path, const std::string& rootPath,
                                         const std::string& mapPath)
{
    const auto asset = PrepareAssetFromFile(path, rootPath, mapPath);

    if (!asset)
    {
        return nullptr;
    }

    if (asset->GetLoadMode() == AssetLoadMode::MultiThreadPrepare)
    {
        // Unlike LoadDirectory, we're only going to be doing single threaded asset load here.

        if (!LoadAssetDataFromFile(asset, LoadThreadingModeContext::SingleThread, AssetLoadStage::Prepare))
            return nullptr;

        if (!LoadAssetDataFromFile(asset, LoadThreadingModeContext::SingleThread, AssetLoadStage::Finish))
            return nullptr;
    }
    else
    {
        if (!LoadAssetDataFromFile(asset, LoadThreadingModeContext::SingleThread, AssetLoadStage::Default))
            return nullptr;
    }

    return asset;
}

//Pine::IAsset* Pine::Assets::LoadFromFile(const std::filesystem::path& path, const std::string& mapPath)
//{
//    return LoadFromFile(path, "", mapPath);
//}

int Pine::Assets::LoadDirectory(const std::filesystem::path& path, bool useAsRelativePath)
{
    constexpr int LOAD_THREAD_COUNT = 6;

    if (!std::filesystem::exists(path))
        return -1;

    // First gather a list of everything we need to load

    std::vector<IAsset*> loadPool;

    for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(path))
    {
        if (dirEntry.is_directory())
            continue;

        auto asset = PrepareAssetFromFile(dirEntry.path(), useAsRelativePath ? path.string() : "", "");
        if (asset == nullptr)
            continue;

        loadPool.push_back(asset);
    }

    // Sort everything

    std::vector<IAsset*> singleThreadLoadAssets;
    std::vector<IAsset*> multiThreadLoadAssets;

    for (const auto& asset : loadPool)
    {
        switch (asset->GetLoadMode())
        {
        case AssetLoadMode::SingleThread:
            singleThreadLoadAssets.push_back(asset);
            break;
        case AssetLoadMode::MultiThread:
        case AssetLoadMode::MultiThreadPrepare:
            multiThreadLoadAssets.push_back(asset);
            break;
        }
    }

    const int multiThreadAssetCount = static_cast<int>(multiThreadLoadAssets.size());

    // Make sure we're not overdoing it, but if this is true
    // we're probably overdoing it anyway.

    int threadAmount = LOAD_THREAD_COUNT;
    if (threadAmount > multiThreadAssetCount)
    {
        threadAmount = multiThreadAssetCount;
    }

    std::vector<std::thread> loadThreads;

    // Maybe "multiThreadLoadAssets" could be passed through the capture list instead (?)

    const auto loadThread = [](const std::vector<IAsset*>& multiThreadLoadAssets, int startCount, int endCount)
    {
        for (int i = startCount;i < endCount;i++)
        {
            auto asset = multiThreadLoadAssets[i];

            if (asset->GetLoadMode() == AssetLoadMode::MultiThreadPrepare)
                LoadAssetDataFromFile(asset, LoadThreadingModeContext::MultiThread, AssetLoadStage::Prepare);
            else
                LoadAssetDataFromFile(asset, LoadThreadingModeContext::MultiThread, AssetLoadStage::Default);
        }
    };

    // Attempt to split up the workload between the available threads
    // This obviously won't be perfect as some assets may require more time to load.

    int startCount = 0;
    for (int i = 0; i < threadAmount;i++)
    {
        int processAmount = static_cast<int>(std::floor(multiThreadAssetCount / threadAmount));
        int endCount = startCount + processAmount;

        if (i == threadAmount - 1)
            endCount = multiThreadAssetCount;

        loadThreads.emplace_back(loadThread, std::cref(multiThreadLoadAssets), startCount, endCount);

        startCount += processAmount;
    }

    // Start doing the single threaded load while we're waiting

    for (auto asset : singleThreadLoadAssets)
    {
        LoadAssetDataFromFile(asset, LoadThreadingModeContext::SingleThread, AssetLoadStage::Default);
    }

    // Now we're forced to wait for the threads to exit before continuing

    for (auto& thread : loadThreads)
    {
        thread.join();
    }

    // Now we have to finish the load for LoadType::MultiThreadPrepare, which means running the remaining bit
    // on the main thread.
    for (auto asset : multiThreadLoadAssets)
    {
        if (asset->GetLoadMode() != AssetLoadMode::MultiThreadPrepare)
            continue;

        LoadAssetDataFromFile(asset, LoadThreadingModeContext::SingleThread, AssetLoadStage::Finish);
    }

    return 0;
}

Pine::IAsset* Pine::Assets::GetAsset(const std::string& path)
{
    if (m_Assets.count(path))
        return nullptr;

    return m_Assets[path];
}
#include "Assets.hpp"

#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/Assets/Material/Material.hpp"
#include "Pine/Assets/Blueprint/Blueprint.hpp"
#include "Pine/Assets/Font/Font.hpp"
#include "Pine/Assets/InvalidAsset/InvalidAsset.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/Assets/Shader/Shader.hpp"
#include "Pine/Assets/Texture2D/Texture2D.hpp"
#include "Pine/Assets/Tilemap/Tilemap.hpp"
#include "Pine/Assets/Tileset/Tileset.hpp"
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Core/String/String.hpp"
#include "Pine/Engine/Engine.hpp"
#include "Pine/Assets/Texture3D/Texture3D.hpp"
#include "Pine/Assets/AudioFile/AudioFile.hpp"
#include "Pine/Assets/CSharpScript/CSharpScript.hpp"

#include <cmath>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <mutex>

using namespace Pine;

namespace
{
    AssetManagerState m_State;

    // All assets currently registered by Pine, they don't have to be
    // valid assets, or loaded assets.
    std::unordered_map<std::string, IAsset*> m_Assets;

    // These are a read-only mirrors of `m_Assets`, but has the file path or id as a key instead.
    std::unordered_map<std::string, IAsset*> m_AssetsFilePath;
    std::unordered_map<std::uint32_t, IAsset*> m_AssetsId;

    // Which assets paths we need to resolve to asset pointers during the end of an ongoing load
    std::vector<AssetResolveReference> m_AssetResolveReferences;
    std::mutex m_AssetResolveReferencesMutex;

    // Keeps track of the current incremental asset id
    std::uint32_t m_CurrentId = 0;

    struct AssetFactory
    {
        // The file extension(s) for this asset type
        std::vector<std::string> m_FileExtensions;

        AssetType m_Type;

        // This function should create the asset object itself
        std::function<IAsset*()> m_Factory;
    };

    std::vector m_AssetFactories = {
        AssetFactory( { { "png", "jpg", "jpeg", "tga", "bmp", "gif" }, AssetType::Texture2D, [](){ return new Texture2D(); } } ),
        AssetFactory( { { "cmap" }, AssetType::Texture3D, [](){ return new Texture3D(); } } ),
        AssetFactory( { { "obj", "fbx", "glb" }, AssetType::Model, [](){ return new Model(); } } ),
        AssetFactory( { { "mat" }, AssetType::Material, [](){ return new Material(); } } ),
        AssetFactory( { { "ttf" }, AssetType::Font, [](){ return new Font(); } } ),
        AssetFactory( { { "shader" }, AssetType::Shader, [](){ return new Shader(); } } ),
        AssetFactory( { { "bpt" }, AssetType::Blueprint, [](){ return new Blueprint(); } } ),
        AssetFactory( { { "lvl" }, AssetType::Level, [](){ return new Level(); } } ),
        AssetFactory( { { "tset" }, AssetType::Tileset, [](){ return new Tileset(); } } ),
        AssetFactory( { { "tmap" }, AssetType::Tilemap, [](){ return new Tilemap(); } } ),
        AssetFactory( { { "wav", "wave", "flac", "ogg", "oga", "spx" }, AssetType::Audio, [](){ return new AudioFile(); } } ),
        AssetFactory( {{ "cs" }, AssetType::CSharpScript, [](){ return new CSharpScript(); } } )
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

        // Remove Windows style separators with a forward slash
        auto filePath = String::Replace(path.string(), "\\", "/");

        // If we don't want to 'change' the relative directory,
        // we can just return the file path itself.
        if (rootPath.empty())
            return filePath;

        if (String::StartsWith(filePath, rootPath + "/"))
            return filePath.substr(rootPath.size() + 1);

        return filePath;
    }

    IAsset* FindExistingAssetFromFile(const std::filesystem::path& filePath, const std::string& rootPath, const std::string& mapPath)
    {
        const auto path = GetAssetMapPath(filePath, rootPath, mapPath);

        if (m_Assets.count(path) == 0)
            return nullptr;

        return m_Assets[path];
    }

    IAsset* PrepareAssetFromFile(const std::filesystem::path& filePath, const std::string& rootPath, const std::string& mapPath)
    {
        if (!exists(filePath))
            return nullptr;

        const auto path = GetAssetMapPath(filePath, rootPath, mapPath);

        if (m_Assets.count(path) != 0)
            return nullptr;

        const auto factory = GetAssetFactoryFromFileName(filePath.filename());

        IAsset* asset;

        if (factory != nullptr)
            asset = factory->m_Factory();
        else
            asset = new InvalidAsset();

        asset->SetFilePath(filePath, rootPath);
        asset->SetPath(path);
        asset->SetId(m_CurrentId++);

        asset->LoadMetadata();

        return asset;
    }

    // More or less which thread we're currently on, SingleThread in this context means
    // only the main thread, i.e., the thread that the OpenGL context is on.
    enum class LoadThreadingModeContext
    {
        SingleThread,
        MultiThread
    };

    bool LoadAssetDataFromFile(IAsset* asset, LoadThreadingModeContext threadingModeContext, AssetLoadStage loadStage)
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

void Assets::Setup()
{
}

void Assets::Shutdown()
{
    for (auto& [path, asset] : m_Assets)
    {
        Log::Verbose(fmt::format("[Assets] Disposing asset {}...", path));

        if (!asset->IsDeleted())
            asset->Dispose();
    }
}

IAsset* Assets::LoadFromFile(const std::filesystem::path& path, const std::string& rootPath,
                             const std::string& mapPath)
{
    bool hasExistingAsset = false;
    IAsset* asset;

    if (auto existingAsset = FindExistingAssetFromFile(path, rootPath, mapPath))
    {
        if (!existingAsset->HasBeenUpdated())
        {
            return existingAsset;
        }

        asset = existingAsset;

        asset->Dispose();

        hasExistingAsset = true;
    }
    else
    {
        asset = PrepareAssetFromFile(path, rootPath, mapPath);
    }

    if (!asset)
    {
        return nullptr;
    }

    m_State = AssetManagerState::LoadFile;

    if (asset->GetLoadMode() == AssetLoadMode::MultiThreadPrepare)
    {
        // Unlike LoadDirectory, we're only going to be doing single threaded asset load here.

        if (!(LoadAssetDataFromFile(asset, LoadThreadingModeContext::SingleThread, AssetLoadStage::Prepare) && 
              LoadAssetDataFromFile(asset, LoadThreadingModeContext::SingleThread, AssetLoadStage::Finish)))
        {
            m_State = AssetManagerState::Idle;

            if (!hasExistingAsset)
            {
                asset->DestroyScriptHandle();
                delete asset;
            }

            return nullptr;
        }
    }
    else
    {
        if (!LoadAssetDataFromFile(asset, LoadThreadingModeContext::SingleThread, AssetLoadStage::Default))
        {
            m_State = AssetManagerState::Idle;

            if (!hasExistingAsset)
            {
                asset->DestroyScriptHandle();
                delete asset;
            }

            return nullptr;
        }
    }

    asset->MarkAsUpdated();

    if (asset->GetScriptHandle()->Object == nullptr)
    {
        asset->CreateScriptHandle();
    }

    m_Assets[asset->GetPath()] = asset;
    m_AssetsFilePath[asset->GetFilePath().string()] = asset;
    m_AssetsId[asset->GetId()] = asset;

    return asset;
}

int Assets::LoadDirectory(const std::filesystem::path& directoryPath, bool useAsRelativePath)
{
    if (!exists(directoryPath))
    {
        Log::Warning(fmt::format("[Assets] Loaded no assets from '{}', directory does not exist.", directoryPath.string()));
        return -1;
    }

    m_State = AssetManagerState::LoadDirectory;

    // First gather a list of everything we need to load
    std::vector<IAsset*> loadPool;

    for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(directoryPath))
    {
        if (dirEntry.is_directory())
            continue;

        IAsset* asset;

        if (auto existingAsset = FindExistingAssetFromFile(dirEntry.path(), useAsRelativePath ? directoryPath.string() : "", ""))
        {
            if (existingAsset->GetType() == AssetType::Invalid)
            {
                continue;
            }

            if (!existingAsset->HasBeenUpdated())
            {
                continue;
            }

            asset = existingAsset;

            asset->Dispose();
        }
        else
        {
            asset = PrepareAssetFromFile(dirEntry.path(), useAsRelativePath ? directoryPath.string() : "", "");
        }

        if (asset == nullptr)
        {
            continue;
        }

        loadPool.push_back(asset);
    }

    // Sort everything
    std::vector<IAsset*> singleThreadLoadAssets;
    std::vector<IAsset*> multiThreadLoadAssets;
    std::vector<IAsset*> dependencyAssets;

    for (const auto& asset : loadPool)
    {
        // If the asset has dependencies, we have to postpone loading the asset and load everything else
        // and hope that it's dependencies has loaded.
        if (asset->HasDependencies())
        {
            dependencyAssets.push_back(asset);
            continue;
        }

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
    int threadAmount = 4;
    if (threadAmount > multiThreadAssetCount)
    {
        threadAmount = multiThreadAssetCount;
    }

    std::vector<std::thread> loadThreads;

    // Maybe "multiThreadLoadAssets" could be passed through the capture list instead (?)
    // Important part is that it's marked as const
    const auto loadThread = [](const std::vector<IAsset*>& multiThreadLoadAssets, int startIndex, int endIndex)
    {
        for (int i = startIndex;i < endIndex;i++)
        {
            auto asset = multiThreadLoadAssets[i];

            if (asset->GetLoadMode() == AssetLoadMode::MultiThreadPrepare)
                LoadAssetDataFromFile(asset, LoadThreadingModeContext::MultiThread, AssetLoadStage::Prepare);
            else
                LoadAssetDataFromFile(asset, LoadThreadingModeContext::MultiThread, AssetLoadStage::Default);
        }
    };

    // TODO: Move this crap to the threadpool worker system.
    if (!multiThreadLoadAssets.empty())
    {
        // Attempt to split up the workload between the available threads
        // This obviously won't be perfect as some assets may require more time to load.
        int threadProcessAmount = static_cast<int>(std::floor(multiThreadAssetCount / threadAmount));

        int startIndex = 0;
        for (int i = 0; i < threadAmount;i++)
        {
            int endIndex = startIndex + threadProcessAmount;

            // If it's the last thread, make sure to load the possibly
            // remaining bit as well.
            if (i == threadAmount - 1)
                endIndex = multiThreadAssetCount;

            loadThreads.emplace_back(loadThread, std::cref(multiThreadLoadAssets), startIndex, endIndex);

            startIndex += threadProcessAmount;
        }
    }

    // Start doing the single threaded load while we're waiting (or not if there aren't any MT assets)
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

    int assetsLoaded = 0;
    int assetsLoadErrors = 0;

    // Finally, add all loaded assets (except dependencies) to our asset map
    for (auto asset : loadPool)
    {
        if (asset->HasDependencies())
        {
            continue;
        }

        if (asset->GetType() != AssetType::Invalid && asset->GetState() != AssetState::Loaded)
        {
            assetsLoadErrors++;

            continue;
        }

        assetsLoaded++;

        asset->MarkAsUpdated();

        if (asset->GetScriptHandle()->Object == nullptr)
        {
            asset->CreateScriptHandle();
        }

        m_Assets[asset->GetPath()] = asset;
        m_AssetsFilePath[asset->GetFilePath().string()] = asset;
        m_AssetsId[asset->GetId()] = asset;
    }

    // At this point we may now load the remaining assets that has dependencies.
    for (const auto asset : dependencyAssets)
    {
        const auto& dependencies = asset->GetDependencies();

        bool missingDependency = false;
        for (const auto& dependency : dependencies)
        {
            if (Get(dependency, false, false) != nullptr)
            {
                continue;
            }

            if (!LoadFromFile(dependency, useAsRelativePath ? directoryPath.string() : "", ""))
            {
                missingDependency = true;
                break;
            }
        }

        if (missingDependency)
        {
            Log::Error("[Assets] Failed to import asset " + asset->GetPath() + ", missing dependency.");
            assetsLoadErrors++;
            continue;
        }
        
        if (asset->GetLoadMode() == AssetLoadMode::MultiThreadPrepare)
        {
            if (!LoadAssetDataFromFile(asset, LoadThreadingModeContext::SingleThread, AssetLoadStage::Prepare))
            {
                assetsLoadErrors++;
                continue;
            }

            if (!LoadAssetDataFromFile(asset, LoadThreadingModeContext::SingleThread, AssetLoadStage::Finish))
            {
                assetsLoadErrors++;
                continue;
            }
        }
        else
        {
            if (!LoadAssetDataFromFile(asset, LoadThreadingModeContext::SingleThread, AssetLoadStage::Default))
            {
                assetsLoadErrors++;
                continue;
            }
        }

        if (asset->GetType() != AssetType::Invalid && asset->GetState() != AssetState::Loaded)
        {
            assetsLoadErrors++;
            continue;
        }

        assetsLoaded++;

        asset->MarkAsUpdated();

        if (asset->GetScriptHandle()->Object == nullptr)
        {
            asset->CreateScriptHandle();
        }

        m_Assets[asset->GetPath()] = asset;
        m_AssetsFilePath[asset->GetFilePath().string()] = asset;
        m_AssetsId[asset->GetId()] = asset;
    }

    // During the load of all assets, some assets may have created resolve references, for assets that may not have
    // been loaded previously, but still needs its pointer set. Since all the loaded assets are in m_Assets now,
    // we may attempt to resolve them to pointers now.
    for (auto& assetResolveReference : m_AssetResolveReferences)
    {
        const auto refMapPath = GetAssetMapPath(assetResolveReference.m_Path, useAsRelativePath ? directoryPath.string() : "", "");
        const auto asset = Get(refMapPath);

        if (!asset)
        {
            // Ignoring warning here since the "Get" function will already warn the user.
            //Log::Warning(fmt::format("Failed to resolve asset reference '{}'.", assetResolveReference.m_Path));
            continue;
        }

        if (assetResolveReference.m_Type != AssetType::Invalid && asset->GetType() != assetResolveReference.m_Type)
        {
            Log::Warning(fmt::format("[Assets] Failed to resolve asset reference '{}', asset type is invalid.", assetResolveReference.m_Path));
            continue;
        }

        *assetResolveReference.m_AssetHandle = asset;
    }

    m_AssetResolveReferences.clear();

    m_State = AssetManagerState::Idle;

    // This is more to provide a warning, since assetsLoadErrors could still be 0, meaning nothing has really gone wrong,
    // however, this is most likely not what the user wanted.
    if (assetsLoaded == 0)
        return -1;

    if (assetsLoadErrors > 0)
        Log::Warning(fmt::format("[Assets] Failed to load {} asset(s) from '{}'.", assetsLoadErrors, directoryPath.string()));

    Log::Verbose("[Assets] Loaded " + std::to_string(assetsLoaded) + " asset(s) from " + directoryPath.string());

    return assetsLoadErrors;
}

void Assets::AddAssetResolveReference(const AssetResolveReference& resolveReference)
{
    std::lock_guard guard(m_AssetResolveReferencesMutex);
    m_AssetResolveReferences.push_back(resolveReference);
}

IAsset* Assets::Get(const std::string& inputPath, bool includeFilePath, bool logWarning)
{
    const auto path = String::Replace(inputPath, "\\", "/");

    // If we want to find the asset by its file path instead of a fake engine path
    if (includeFilePath)
    {
        if (m_AssetsFilePath.count(path) != 0)
            return m_AssetsFilePath[path];
    }

    if (m_Assets.count(path) == 0)
    {
        if (logWarning)
        {
            Log::Warning(fmt::format("[Assets] Failed to find asset by path, {}", inputPath));
        }

        return nullptr;
    }

    return m_Assets[path];
}

IAsset* Assets::GetById(std::uint32_t id)
{
    if (m_AssetsId.count(id) == 0)
        return nullptr;

    return m_AssetsId[id];
}

IAsset *Assets::GetOrLoad(const std::string &inputPath, const bool includeFilePath)
{
    // This should never be called during a LoadDirectory operation!!!
    assert(m_State != AssetManagerState::LoadDirectory);

    const auto path = String::Replace(inputPath, "\\", "/");

    if (includeFilePath)
    {
        if (m_AssetsFilePath.count(path) != 0)
            return m_AssetsFilePath[path];
    }

    if (m_Assets.count(path) == 0)
    {
        return LoadFromFile(path);
    }

    return m_Assets[path];
}

void Assets::MoveAsset(IAsset *asset, const std::filesystem::path &newFilePath)
{
    const auto newPath = GetAssetMapPath(newFilePath, asset->GetFileRootPath().string(), "");
    const auto oldPath = asset->GetPath();
    const auto oldFilePath = asset->GetFilePath();

    asset->SetPath(newPath);
    asset->SetFilePath(newFilePath, String::StartsWith(newFilePath.string(), asset->GetFileRootPath().string()) ? asset->GetFileRootPath() : "");

    m_Assets.erase(oldPath);
    m_AssetsFilePath.erase(oldFilePath.string());

    m_Assets[newPath] = asset;
    m_AssetsFilePath[newFilePath.string()] = asset;
}

const std::unordered_map<std::string, IAsset*>& Assets::GetAll()
{
    return m_Assets;
}

void Assets::SaveAll()
{
    int savedAssets = 0;

    for (const auto& [path, asset] : m_Assets)
    {
        if (!asset->HasFile())
            continue;
        if (!asset->IsModified())
            continue;

        Log::Verbose(fmt::format("[Assets] Saving asset {}...", asset->GetPath()));

        asset->SaveToFile();
        asset->SaveMetadata();
        asset->MarkAsUpdated();

        savedAssets++;
    }

    Log::Info(fmt::format("[Assets] Saved {} modified assets.", savedAssets));
}

void Assets::RefreshAll()
{
    for (const auto& [path, asset] : m_Assets)
    {
        if (!asset->HasFile())
            continue;
        if (asset->IsDeleted())
            continue;

        if (!std::filesystem::exists(asset->GetFilePath()))
        {
            Log::Warning(fmt::format("Assets: Asset '{}' has been deleted from disk.", asset->GetPath()));

            asset->MarkAsDeleted();
            asset->Dispose();
        }

        // TODO: Not sure how I want to handle this just yet. We shouldn't have to deal with cases like this for now anyway.
        // Realistically only the editor will call this, and it will already reload the assets from the directory right after this is called,
        // which will deal with updated assets in a nicer way.
    }
}

AssetManagerState Assets::GetState()
{
    return m_State;
}


#pragma once

#include "Pine/Script/Factory/ScriptObjectFactory.hpp"
#include <cstdint>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>

namespace Pine
{

    // All asset types in Pine, as for how you could extend this to add "custom"
    // asset types is still unclear.
    enum class AssetType
    {
        Invalid,
        Blueprint,
        Level,
        Material,
        Mesh,
        Model,
        Shader,
        Texture2D,
        Texture3D,
        Font,
        Tileset,
        Tilemap,
        Audio,
        CSharpScript,
        Terrain,
        Count
    };

    inline const char *AssetTypeToString(AssetType type)
    {
        switch (type)
        {
        case AssetType::Invalid:
            return "Invalid";
        case AssetType::Blueprint:
            return "Blueprint";
        case AssetType::Level:
            return "Level";
        case AssetType::Material:
            return "Material";
        case AssetType::Mesh:
            return "Mesh";
        case AssetType::Model:
            return "Model";
        case AssetType::Shader:
            return "Shader";
        case AssetType::Texture2D:
            return "Texture2D";
        case AssetType::Texture3D:
            return "Texture3D";
        case AssetType::Font:
            return "Font";
        case AssetType::Tileset:
            return "Tileset";
        case AssetType::Tilemap:
            return "Tilemap";
        case AssetType::Audio:
            return "Audio";
        case AssetType::CSharpScript:
            return "Script";
        case AssetType::Terrain:
            return "Terrain";
        default:
            return "Unknown";
        }
    }

    enum class AssetState
    {
        Unloaded, // The engine knows about the asset's existence, but hasn't loaded it at all.
        Preparing, // If the asset is in the process of being loaded, probably used with `MultiThreadPrepare`
        Loaded // The asset has been loaded and is ready for use.
    };

    enum class AssetLoadMode
    {
        SingleThread, // Loads the asset in order on the main thread
        MultiThread, // Allows the asset to be loaded in its own thread concurrently.
        MultiThreadPrepare // Used for GPU assets, uploads data to GPU in main thread only.
    };

    enum class AssetLoadStage
    {
        Default,
        Prepare,
        Finish
    };

    template<class T>
    class AssetHandle;

    class Asset
    {
    protected:
        std::string m_FileName;

        AssetType m_Type = AssetType::Invalid;

        // The asset's fake path within the engine, does not exactly mean
        // the path of the file on the drive.
        std::string m_Path;

        // Unique id for this asset for faster lookups
        std::uint32_t m_Id;

        // If this asset has a corresponding file path, and should be a file.
        // Otherwise, it may have been generated during run-time.
        bool m_HasFile = false;
        std::filesystem::path m_FilePath;
        std::filesystem::path m_FileRootPath;

        // Used to determine if an asset file has been updated on the disk since we last loaded it.
        std::filesystem::file_time_type m_DiskWriteTime;

        // Used to determine if this asset has been modified, and that we'd like to save it.
        bool m_HasBeenModified = false;

        AssetState m_State = AssetState::Unloaded;

        AssetLoadMode m_LoadMode = AssetLoadMode::SingleThread;

        // It's up to the asset to set this to true if needed. If it's true, the user may put any data
        // into m_Metadata and the asset manager should take care of loading/saving the data automatically.
        bool m_HasMetadata = false;
        nlohmann::json m_Metadata;

        // Setting this to true may be required if your asset needs to do processing on other assets, i.e. the other
        // assets are required to have been loaded before loading this. Filling m_DependencyFiles is also up to the
        // asset itself. If just a reference to an asset is required, consider looking into AssetResolveReference.
        // Specifying dependencies will also make the asset manager load the dependencies first,
        // however this is not required.
        bool m_HasDependencies = false;
        std::vector<std::string> m_DependencyFiles;

        int m_ReferenceCount = 0;
        bool m_IsDeleted = false;

        Script::ObjectHandle m_ScriptObjectHandle = { nullptr, 0 };

        template<typename>
        friend class AssetHandle;
    public:
        virtual ~Asset() = default;

        void SetId(std::uint32_t id);
        std::uint32_t GetId() const;

        void CreateScriptHandle();
        void DestroyScriptHandle();

        Script::ObjectHandle* GetScriptHandle();

        const std::string &GetFileName() const;
        AssetType GetType() const;

        void SetPath(const std::string &path);
        const std::string &GetPath() const;

        void SetFilePath(const std::filesystem::path &path);
        void SetFilePath(const std::filesystem::path &path, const std::filesystem::path &root);

        const std::filesystem::path &GetFilePath() const;
        const std::filesystem::path &GetFileRootPath() const;

        virtual void MarkAsUpdated();
        virtual bool HasBeenUpdated() const;

        bool HasFile() const;
        bool HasMetadata() const;
        bool HasDependencies() const;

        void MarkAsDeleted();
        void MarkAsModified();

        bool IsDeleted() const;
        bool IsModified() const;

        AssetState GetState() const;
        AssetLoadMode GetLoadMode() const;

        void LoadMetadata();
        void SaveMetadata();

        const std::vector<std::string> &GetDependencies() const;

        virtual bool LoadFromFile(AssetLoadStage stage = AssetLoadStage::Default);
        virtual bool SaveToFile();
        virtual void Dispose() = 0;
    };

    template<class T>
    class AssetHandle
    {
    private:
        mutable T *m_Asset = nullptr;
    public:
        T *Get() const
        {
            // Make sure to remove any pending deletion assets
            if (m_Asset)
            {
                if (reinterpret_cast<Asset *>(m_Asset)->m_IsDeleted)
                {
                    --reinterpret_cast<Asset *>(m_Asset)->m_ReferenceCount;

                    m_Asset = nullptr;
                }
            }

            return m_Asset;
        }

        T *operator->()
        {
            return m_Asset;
        }

        AssetHandle &operator=(Asset *asset)
        {
            // Decrease the ref count on the asset we already have
            if (m_Asset != nullptr)
                --reinterpret_cast<Asset *>(m_Asset)->m_ReferenceCount;

            // Assign the new asset
            m_Asset = static_cast<T *>(asset);

            // Make sure the new asset updates its reference count
            if (asset != nullptr)
                ++reinterpret_cast<Asset *>(m_Asset)->m_ReferenceCount;

            return *this;
        }

        inline bool operator==(const Asset *b)
        {
            return m_Asset == b;
        }
    };

    struct AssetResolveReference
    {
        std::string m_Path;
        AssetHandle<Asset> *m_AssetHandle;
        AssetType m_Type = AssetType::Invalid;
    };
}

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <nlohmann/json.hpp>

#include "Pine/Core/UId/UId.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"
#include "Pine/Script/Factory/ScriptObjectFactory.hpp"

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
        Loaded // The asset has been loaded and is ready for use.
    };

    template<class T>
    class AssetHandle;

    struct AssetSource
    {
        std::string FilePath;
        std::uint64_t LastWriteTime;
    };

    class Asset
    {
    protected:
        UId m_UId;

        AssetType m_Type = AssetType::Invalid;

        // Mapped engine path within the "VFS"
        std::string m_Path{};

        // Underlying ".passet" file path
        std::filesystem::path m_FilePath{};

        AssetState m_State = AssetState::Unloaded;

        // Source files that this asset was initially imported from. This is only present
        // within the editor and such, not the final runtime.
        std::vector<AssetSource> m_SourceFiles;

        // Basically refers to data dependencies, in the way that this asset cannot be loaded/processed
        // until the specified assets are loaded beforehand.
        std::vector<UId> m_Dependencies;

        int m_ReferenceCount = 0;
        bool m_IsDeleted = false;

        bool m_HasBeenModified = false;

        Script::ObjectHandle m_ScriptObjectHandle = { nullptr, 0 };

        virtual bool LoadAssetData(const ByteSpan& span);
        virtual ByteSpan SaveAssetData();

        struct AssetSourceSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_STRING(FilePath);
            PINE_SERIALIZE_PRIMITIVE(LastWriteTime, Serialization::DataType::Int64);
        };

        struct AssetSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_PRIMITIVE(UId, Serialization::DataType::UId);
            PINE_SERIALIZE_PRIMITIVE(Type, Serialization::DataType::Int32);
            PINE_SERIALIZE_STRING(Path);
            PINE_SERIALIZE_ARRAY(Sources);
            PINE_SERIALIZE_ARRAY_FIXED(Dependencies, Pine::UId);
            PINE_SERIALIZE_DATA(Data);
        };

        template<typename>
        friend class AssetHandle;
    public:
        virtual ~Asset() = default;

        // This should ideally be done through a constructor.
        void SetupNew(const std::string& path);

        const UId& GetUId() const;
        const AssetType& GetType() const;
        const AssetState& GetState() const;

        const std::string& GetPath() const;
        const std::filesystem::path& GetFilePath() const;

        void AddSource(const std::string& filePath);
        void RemoveSource(const std::string& filePath);
        const std::vector<AssetSource>& GetSources() const;

        void MarkAsModified();
        bool HasBeenModified() const;

        void CreateScriptHandle();
        void DestroyScriptHandle();
        Script::ObjectHandle* GetScriptHandle();

        virtual bool Import();
        virtual void Dispose() = 0;

        ByteSpan Save();
        void SaveToFile();

        static Asset* Load(const ByteSpan& data, bool ignoreAssetData = false);
        static Asset* Load(const ByteSpan& data, const std::string& filePath, bool ignoreAssetData = false);
        static Asset* LoadFromFile(const std::filesystem::path& filePath, bool ignoreAssetData = false);
    };

    namespace Assets
    {
        Asset* GetAssetByUId(UId id);
    }

    template<class TAsset>
    class AssetHandle
    {
    private:
        UId m_UId;

        // cached pointer to the underlying asset.
        mutable TAsset *m_Asset = nullptr;
    public:
        AssetHandle() = default;

        explicit AssetHandle(UId id) : m_UId(id)
        {
        }

        TAsset *Get() const
        {
            // Make sure to validate that the handle is still valid.
            if (m_Asset)
            {
                auto assetType = reinterpret_cast<Asset *>(m_Asset);

                if (assetType->m_IsDeleted)
                {
                    --reinterpret_cast<Asset *>(m_Asset)->m_ReferenceCount;
                    m_Asset = nullptr;
                }
                else if (assetType->GetUId() != m_UId)
                {
                    m_Asset = nullptr;
                }
            }

            // If not cached, attempt to get a pointer.
            if (!m_Asset)
            {
                m_Asset = dynamic_cast<TAsset*>(Assets::GetAssetByUId(m_UId));
            }

            return m_Asset;
        }

        TAsset *operator->()
        {
            return m_Asset;
        }

        AssetHandle &operator=(Asset *asset)
        {
            // Decrease the ref count on the asset we might already have
            if (m_Asset != nullptr)
            {
                --reinterpret_cast<Asset *>(m_Asset)->m_ReferenceCount;
            }

            if (asset != nullptr)
            {
                m_UId = asset->GetUId();
                m_Asset = static_cast<TAsset *>(asset);

                ++reinterpret_cast<Asset *>(m_Asset)->m_ReferenceCount;
            }
            else
            {
                m_Asset = nullptr;
                m_UId = UId::Empty();
            }

            return *this;
        }

        AssetHandle &operator=(UId id)
        {
            m_UId = id;
            m_Asset = nullptr;

            return *this;
        }

        bool operator==(const Asset *b) const
        {
            return m_Asset == b;
        }

        bool operator==(const AssetHandle &b) const
        {
            return m_UId == b.m_UId;
        }

        bool operator==(const UId &id) const
        {
            return m_UId == id;
        }

        bool operator!=(const UId& id) const
        {
            return m_UId != id;
        }
    };
}

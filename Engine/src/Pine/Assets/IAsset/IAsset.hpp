#pragma once
#include <string>
#include <filesystem>

namespace Pine
{

    // All asset types in Pine, as for how you could extend this to add "custom"
    // asset types is still unclear.
    enum class AssetType
    {
        Invalid,
        Material,
        Mesh,
        Model,
        Shader,
        Texture2D,
        Texture3D,
        Count
    };

    inline const char* AssetTypeToString(AssetType type)
    {
        switch (type)
        {
        case AssetType::Invalid:
            return "Invalid";
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
        default:
            return "Unknown";
        }
    }

    enum class AssetState
    {
        Unloaded, // The engine knows about the asset's existence, but hasn't loaded it at all.
        Loaded // The asset has been loaded and is ready for use.
    };

    template <class T>
    class AssetContainer;

    class IAsset
    {
    protected:
        std::string m_FileName;
        AssetType m_Type = AssetType::Invalid;

        // The asset's fake path within the engine, does not exactly mean
        // the path of the file on the drive.
        std::string m_Path;

        // If this asset has a corresponding file path, and should be a file.
        // Otherwise, it may have been generated during run-time.
        bool m_HasFile = false;
        std::filesystem::path m_FilePath;

        AssetState m_State = AssetState::Unloaded;

        int m_ReferenceCount = 0;
        bool m_IsDeleted = false;

        friend class AssetContainer<IAsset>;
    public:
        virtual ~IAsset() = default;

        const std::string& GetFileName() const;

        AssetType GetType() const;

        void SetPath(const std::string& path);
        const std::string& GetPath() const;

        void SetFilePath(std::filesystem::path path);
        const std::filesystem::path& GetFilePath() const;

        AssetState GetState() const;

        virtual bool LoadFromFile();
        virtual bool SaveToFile();

        virtual void Dispose() = 0;
    };

    template <class T>
    class AssetContainer
    {
        mutable T m_Asset = nullptr;

        T Get() const
        {
            if (m_Asset)
            {
                if (reinterpret_cast<IAsset*>(m_Asset)->m_IsDeleted)
                {
                    reinterpret_cast<IAsset*>(m_Asset)->m_ReferenceCount--;

                    m_Asset = nullptr;
                }
            }

            return m_Asset;
        }

        AssetContainer& operator=(IAsset* asset)
        {
            // Decrease the ref count on the asset we already have
            reinterpret_cast<IAsset*>(m_Asset)->m_ReferenceCount--;

            // Assign the new asset
            m_Asset = static_cast<T>(asset);

            // Make sure the new asset updates its reference count
            reinterpret_cast<IAsset*>(m_Asset)->m_ReferenceCount++;

            return *this;
        }

        inline bool operator==(const IAsset* b)
        {
            return m_Asset == b;
        }
    };


}

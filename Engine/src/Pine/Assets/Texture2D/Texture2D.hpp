#pragma once

#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Graphics/Interfaces/ITexture.hpp"

namespace Pine
{
    namespace Importer
    {
        class TextureImporter;
    }

    enum class TextureUsageHint
    {
        Albedo = 0,
        AlbedoFaster,
        NormalMap,
        Grayscale,
        Uncompressed
    };

    enum class TextureCompressionQuality
    {
        Normal,
        Fastest,
        Production
    };

    struct TextureImportConfiguration
    {
        TextureUsageHint UsageHint = TextureUsageHint::AlbedoFaster;
        TextureCompressionQuality CompressionQuality = TextureCompressionQuality::Normal;
        bool GenerateMipmaps = false;
    };

    class Texture2D : public Asset
    {
    private:
        // General texture information
        int m_Width = 0;
        int m_Height = 0;
        int m_MipmapLevels = 0;

        Graphics::TextureFormat m_Format = Graphics::TextureFormat::SingleChannel;

        Graphics::TextureFilteringMode m_FilteringMode = Graphics::TextureFilteringMode::Linear;
        Graphics::TextureFilteringMode m_MipFilteringMode = Graphics::TextureFilteringMode::Linear;

        Graphics::TextureWrapMode m_WrapMode = Graphics::TextureWrapMode::Repeat;

        Graphics::TextureCompressionFormat m_CompressionFormat = Graphics::TextureCompressionFormat::Raw;

        // Underlying graphics texture
        Graphics::ITexture* m_Texture = nullptr;

        // Texture importing information
        void* m_TextureData = nullptr;
        size_t m_TextureDataSize = 0;
        TextureImportConfiguration m_ImportConfiguration;

        struct TextureSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_PRIMITIVE(Width, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(Height, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(TextureFormat, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(FilteringMode, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(MipFilteringMode, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(WrapMode, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(CompressionFormat, Serialization::DataType::Int32);

            PINE_SERIALIZE_PRIMITIVE(ImportUsageHint, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(ImportCompressionQuality, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(ImportGenerateMipMaps, Serialization::DataType::Boolean);

            PINE_SERIALIZE_ARRAY(Data);
        };

        bool LoadAssetData(const ByteSpan& span) override;
        ByteSpan SaveAssetData() override;
    public:
        Texture2D();

        int GetWidth() const;
        int GetHeight() const;
        int GetMipmapLevels() const;

        Graphics::TextureFormat GetFormat() const;
        Graphics::TextureCompressionFormat GetCompressionFormat() const;

        void SetFilteringMode(Graphics::TextureFilteringMode textureFilteringMode);
        Graphics::TextureFilteringMode GetFilteringMode() const;

        void SetMipFilteringMode(Graphics::TextureFilteringMode textureFilteringMode);
        Graphics::TextureFilteringMode GetMipFilteringMode() const;

        void SetWrapMode(Graphics::TextureWrapMode wrapMode);
        Graphics::TextureWrapMode GetWrapMode() const;

        TextureImportConfiguration& GetImportConfiguration();

        Graphics::ITexture* GetGraphicsTexture() const;

        bool HasTextureData() const;
        ByteSpan GetTextureData() const;

        bool Import() override;
        void Dispose() override;

        friend class Importer::TextureImporter;
    };

}
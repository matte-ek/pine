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

    enum class TextureCompressionFormat
    {
        Raw = 0,
        BC1,
        BC4,
        BC5,
        BC7
    };

    struct TextureImportConfiguration
    {
        TextureUsageHint UsageHint = TextureUsageHint::Albedo;
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
        TextureCompressionFormat m_CompressionFormat = TextureCompressionFormat::Raw;

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
            PINE_SERIALIZE_PRIMITIVE(CompressionFormat, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(ImportUsageHint, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(ImportGenerateMipMaps, Serialization::DataType::Boolean);
            PINE_SERIALIZE_ARRAY(Data);
        };

        bool LoadAssetData(const ByteSpan& span) override;
    public:
        Texture2D();

        int GetWidth() const;
        int GetHeight() const;
        int GetMipmapLevels() const;

        Graphics::TextureFormat GetFormat() const;
        Graphics::ITexture* GetGraphicsTexture() const;

        bool HasTextureData() const;
        ByteSpan GetTextureData() const;

        bool Import() override;
        ByteSpan Save() override;

        void Dispose() override;

        friend class Importer::TextureImporter;
    };

}
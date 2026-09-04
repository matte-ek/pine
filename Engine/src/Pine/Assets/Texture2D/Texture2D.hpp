#pragma once

#include <optional>

#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Assets/Importer/AssetImporter.hpp"
#include "Pine/Graphics/Interfaces/ITexture.hpp"

namespace Pine
{
    namespace Importer
    {
        class TextureImporter;
    }

    // What a texture is for. Decides the block compression format it is encoded with, and whether
    // the GPU sRGB-decodes it when sampled. Serialized as an integer, so only ever append.
    enum class TextureUsageHint
    {
        Albedo = 0,
        AlbedoFaster,
        NormalMap,
        Grayscale,
        DataMap,
        Uncompressed,

        // A three-channel texture that carries data rather than colour: packed
        // roughness/metalness/AO, specular maps. Encoded like Albedo, but sampled linearly -
        // sRGB-decoding these makes the values they hold plain wrong.
        LinearColor
    };

    // Why a texture carries the usage hint it does. Ordered weakest to strongest: a hint is only
    // ever overwritten by a reason at least as good as the one already recorded, which is what
    // keeps a re-import from throwing away a choice the user made by hand. Serialized alongside
    // the hint so the ranking survives one.
    enum class TextureUsageHintSource
    {
        // Nobody had an opinion; this is whatever the configuration was constructed with.
        Default = 0,

        // Worked out from the file name (see GuessTextureUsageHint).
        Heuristic,

        // The source file said so outright - e.g. a model file listing a texture as its normal
        // map. Ground truth as far as the importer is concerned.
        SourceFormat,

        // Set by hand.
        User
    };

    enum class TextureCompressionQuality
    {
        Normal,
        Fastest,
        Production
    };

    struct TextureImportConfiguration : AssetImportConfiguration
    {
        TextureUsageHint UsageHint = TextureUsageHint::AlbedoFaster;
        TextureUsageHintSource UsageHintSource = TextureUsageHintSource::Default;
        TextureCompressionQuality CompressionQuality = TextureCompressionQuality::Normal;
        bool GenerateMipmaps = true;
    };

    // Applies 'hint' to 'configuration' if 'source' is at least as good a reason to believe it as
    // whatever decided the hint already there, and reports whether it did. Every guess about what
    // a texture is for goes through here - the model importer reading a texture slot out of a
    // model file, the file name heuristic, the editor writing down what the user picked - so that
    // a weaker reason can never quietly replace a stronger one.
    bool ApplyTextureUsageHint(
        TextureImportConfiguration& configuration,
        TextureUsageHint hint,
        TextureUsageHintSource source);

    // The heuristic tier: what a texture is probably for, going by its file name and the directory
    // holding it. Returns nothing when nothing matches, which is the common case - most textures
    // are albedo and are named after what they depict, not what they are.
    std::optional<TextureUsageHint> GuessTextureUsageHint(const std::filesystem::path& sourcePath);

    struct TextureImportData
    {
        void* m_TextureData = nullptr;
        size_t m_TextureDataSize = 0;

        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
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

        // RAW texture data, if the asset is a DataMap
        void* m_TextureData = nullptr;
        size_t m_TextureDataSize = 0;

        // Texture importing information
        std::vector<TextureImportData> m_ImportData;
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
            PINE_SERIALIZE_PRIMITIVE(ImportUsageHintSource, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(ImportCompressionQuality, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(ImportGenerateMipMaps, Serialization::DataType::Boolean);

            PINE_SERIALIZE_ARRAY(Mips);
        };

        struct TextureMipSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_PRIMITIVE(Width, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(Height, Serialization::DataType::Int32);
            PINE_SERIALIZE_DATA(Data);
        };

    protected:
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

        void* GetTextureData() const;
        size_t GetTextureDataSize() const;

        bool Import(Importer::AssetImport* context) override;
        void ResolveImportSettings(const Importer::AssetImport& import) override;
        void Dispose() override;

        friend class Importer::TextureImporter;
    };

}
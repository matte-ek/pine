#include "Texture2D.hpp"
#include "Pine/Graphics/Graphics.hpp"

#include <algorithm>
#include <cctype>

#include "Importer/TextureImporter.hpp"
#include "Pine/Core/File/File.hpp"
#include "Pine/Threading/Threading.hpp"

namespace
{
    // Color textures (albedo) are authored in sRGB and must be uploaded with an sRGB internal format
    // so the GPU decodes them to linear on sample. Data textures (normal maps, masks, grayscale) are
    // already linear and must stay linear. The usage hint the texture was imported with tells us which.
    bool IsSRGBUsageHint(const Pine::TextureUsageHint hint)
    {
        return hint == Pine::TextureUsageHint::Albedo || hint == Pine::TextureUsageHint::AlbedoFaster;
    }

    // Cuts a name into lower-case tokens on the separators texture packs actually use. Matching
    // whole tokens instead of substrings is not optional: 'metal_floor_5.png' in gm's texture pack
    // is an albedo texture of a metal floor, and a contains("metal") rule would encode it as a
    // metalness map.
    std::vector<std::string> Tokenize(const std::string& name)
    {
        std::vector<std::string> tokens;
        std::string current;

        for (const auto character : name)
        {
            if (character == '_' || character == '-' || character == ' ' || character == '.')
            {
                if (!current.empty())
                {
                    tokens.push_back(current);
                    current.clear();
                }

                continue;
            }

            current += static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }

        if (!current.empty())
        {
            tokens.push_back(current);
        }

        return tokens;
    }

    bool IsNumericToken(const std::string& token)
    {
        return !token.empty() && std::all_of(token.begin(), token.end(),
            [](const unsigned char character) { return std::isdigit(character) != 0; });
    }

    // The heuristic table. Deliberately short: every entry here is a token that means "this is a
    // map of type X" and essentially nothing else. The ambiguous short forms that a texture pack
    // also uses to describe a *material* - 'metal', 'rough', 'wood' - are left out on purpose;
    // gm's pack alone has 'floor_3_metal' (albedo) next to 'metal_floor_5' (also albedo).
    struct UsageHintRule
    {
        const char* Token;
        Pine::TextureUsageHint Hint;
    };

    constexpr UsageHintRule UsageHintRules[] = {
        {"n", Pine::TextureUsageHint::NormalMap},
        {"nrm", Pine::TextureUsageHint::NormalMap},
        {"norm", Pine::TextureUsageHint::NormalMap},
        {"normal", Pine::TextureUsageHint::NormalMap},
        {"normals", Pine::TextureUsageHint::NormalMap},
        {"normalmap", Pine::TextureUsageHint::NormalMap},

        // Emitted light is colour, so it stays sRGB - but it is usually mostly black with a few
        // bright regions, which is exactly what BC1 handles worst. The fast default exists to keep
        // bulk albedo imports quick; there are never many emission maps.
        {"emis", Pine::TextureUsageHint::Albedo},
        {"emission", Pine::TextureUsageHint::Albedo},
        {"emissive", Pine::TextureUsageHint::Albedo},

        {"roughness", Pine::TextureUsageHint::LinearColor},
        {"metallic", Pine::TextureUsageHint::LinearColor},
        {"metalness", Pine::TextureUsageHint::LinearColor},
        {"spec", Pine::TextureUsageHint::LinearColor},
        {"specular", Pine::TextureUsageHint::LinearColor},
        {"ao", Pine::TextureUsageHint::LinearColor},
        {"occlusion", Pine::TextureUsageHint::LinearColor},
        {"orm", Pine::TextureUsageHint::LinearColor},
        {"rma", Pine::TextureUsageHint::LinearColor},
        {"arm", Pine::TextureUsageHint::LinearColor},
        {"mask", Pine::TextureUsageHint::LinearColor},
    };

    std::optional<Pine::TextureUsageHint> MatchRule(const std::string& token)
    {
        for (const auto& rule : UsageHintRules)
        {
            if (token == rule.Token)
            {
                return rule.Hint;
            }
        }

        return {};
    }
}

bool Pine::Texture2D::LoadAssetData(const ByteSpan& span)
{
    TextureSerializer textureSerializer;

    if (!textureSerializer.Read(span))
    {
        return false;
    }

    // Load general information about the texture
    textureSerializer.Width.Read(m_Width);
    textureSerializer.Height.Read(m_Height);
    textureSerializer.TextureFormat.Read(m_Format);
    textureSerializer.FilteringMode.Read(m_FilteringMode);
    textureSerializer.MipFilteringMode.Read(m_MipFilteringMode);
    textureSerializer.WrapMode.Read(m_WrapMode);
    textureSerializer.CompressionFormat.Read(m_CompressionFormat);
    textureSerializer.AlphaMode.Read(m_AlphaMode);
    textureSerializer.ImportUsageHint.Read(m_ImportConfiguration.UsageHint);
    textureSerializer.ImportUsageHintSource.Read(m_ImportConfiguration.UsageHintSource);
    textureSerializer.ImportCompressionQuality.Read(m_ImportConfiguration.CompressionQuality);
    textureSerializer.ImportGenerateMipMaps.Read(m_ImportConfiguration.GenerateMipmaps);

    m_MipmapLevels = textureSerializer.Mips.GetDataCount();

    auto task = Threading::QueueTask<void>([this, &textureSerializer]()
    {
        // Since it's running on the main thread, this is "thread-safe".
        if (m_Texture == nullptr)
        {
            m_Texture = Graphics::GetGraphicsAPI()->CreateTexture();
        }

        m_Texture->Bind();

        // Decide sRGB vs linear from the import usage hint, before any upload uses it.
        m_Texture->SetSRGB(IsSRGBUsageHint(m_ImportConfiguration.UsageHint));

        // Prepare and upload texture data to GPU.
        for (size_t i{}; i < textureSerializer.Mips.GetDataCount(); i++)
        {
            TextureMipSerializer mipSerializer;

            if (!mipSerializer.Read(textureSerializer.Mips.GetData(i)))
            {
                PWarning("Failed to read texture mip.");
                continue;
            }

            const auto& mipData = mipSerializer.Data.Read();

            if (m_ImportConfiguration.UsageHint == TextureUsageHint::DataMap)
            {
                m_TextureData = malloc(mipData.size);
                m_TextureDataSize = mipData.size;

                memcpy(m_TextureData, mipData.data, mipData.size);
            }

            if (m_CompressionFormat == Graphics::TextureCompressionFormat::Raw)
            {
                m_Texture->UploadTextureData(
                       mipSerializer.Width.Read<std::uint32_t>(),
                       mipSerializer.Height.Read<std::uint32_t>(),
                       i,
                       m_Format,
                       Graphics::TextureDataFormat::UnsignedByte,
                       mipData.data);
            }
            else
            {
                m_Texture->UploadTextureDataCompressed(
                    mipSerializer.Width.Read<std::uint32_t>(),
                    mipSerializer.Height.Read<std::uint32_t>(),
                    i,
                    m_Format,
                    m_CompressionFormat,
                    mipData.data,
                    mipData.size);
            }
        }

        if (textureSerializer.Mips.GetDataCount() > 1)
        {
            m_Texture->EnableMipmaps(textureSerializer.Mips.GetDataCount() - 1);
            m_Texture->SetMipmapFilteringMode(m_MipFilteringMode);
        }
        else
        {
            m_Texture->SetFilteringMode(m_FilteringMode);
        }

        m_Texture->SetTextureWrapMode(m_WrapMode);
    },
    TaskThreadingMode::MainThread);

    // Wait for the GPU upload jobs to complete.
    Threading::AwaitTaskResult(task);

    // We're done here
    m_State = AssetState::Loaded;

    return true;
}

Pine::Texture2D::Texture2D()
{
    m_Type = AssetType::Texture2D;
}

void Pine::Texture2D::Dispose()
{
    for (const auto& import : m_ImportData)
    {
        free(import.m_TextureData);
    }

    m_ImportData.clear();

    if (m_Texture != nullptr)
    {
        m_Texture->Dispose();
    }

    m_State = AssetState::Unloaded;
}

int Pine::Texture2D::GetWidth() const
{
    return m_Width;
}

int Pine::Texture2D::GetHeight() const
{
    return m_Height;
}

int Pine::Texture2D::GetMipmapLevels() const
{
    return m_MipmapLevels;
}

Pine::Graphics::TextureFormat Pine::Texture2D::GetFormat() const
{
    return m_Format;
}

Pine::Graphics::TextureCompressionFormat Pine::Texture2D::GetCompressionFormat() const
{
    return m_CompressionFormat;
}

Pine::TextureAlphaMode Pine::Texture2D::GetAlphaMode() const
{
    return m_AlphaMode;
}

void Pine::Texture2D::SetFilteringMode(const Graphics::TextureFilteringMode textureFilteringMode)
{
    m_FilteringMode = textureFilteringMode;

    if (m_Texture != nullptr)
    {
        m_Texture->SetFilteringMode(textureFilteringMode);
    }
}

Pine::Graphics::TextureFilteringMode Pine::Texture2D::GetFilteringMode() const
{
    return m_FilteringMode;
}

void Pine::Texture2D::SetMipFilteringMode(const Graphics::TextureFilteringMode textureFilteringMode)
{
    m_MipFilteringMode = textureFilteringMode;

    if (m_Texture != nullptr)
    {
        m_Texture->SetMipmapFilteringMode(textureFilteringMode);
    }
}

Pine::Graphics::TextureFilteringMode Pine::Texture2D::GetMipFilteringMode() const
{
    return m_MipFilteringMode;
}

void Pine::Texture2D::SetWrapMode(const Graphics::TextureWrapMode wrapMode)
{
    m_WrapMode = wrapMode;

    if (m_Texture != nullptr)
    {
        m_Texture->SetTextureWrapMode(wrapMode);
    }
}

Pine::Graphics::TextureWrapMode Pine::Texture2D::GetWrapMode() const
{
    return m_WrapMode;
}

Pine::TextureImportConfiguration& Pine::Texture2D::GetImportConfiguration()
{
    return m_ImportConfiguration;
}

Pine::Graphics::ITexture* Pine::Texture2D::GetGraphicsTexture() const
{
    return m_Texture;
}

bool Pine::Texture2D::HasTextureData() const
{
    return m_TextureData != nullptr;
}

void* Pine::Texture2D::GetTextureData() const
{
    return m_TextureData;
}

size_t Pine::Texture2D::GetTextureDataSize() const
{
    return m_TextureDataSize;
}

bool Pine::Texture2D::Import(Importer::AssetImport* context)
{
    return Importer::TextureImporter::Import(this);
}

void Pine::Texture2D::ResolveImportSettings(const Importer::AssetImport& import)
{
    if (import.SourcePaths.empty())
    {
        return;
    }

    if (const auto hint = GuessTextureUsageHint(import.SourcePaths.front()))
    {
        ApplyTextureUsageHint(m_ImportConfiguration, *hint, TextureUsageHintSource::Heuristic);
    }
}

bool Pine::ApplyTextureUsageHint(
    TextureImportConfiguration& configuration,
    const TextureUsageHint hint,
    const TextureUsageHintSource source)
{
    if (source < configuration.UsageHintSource)
    {
        return false;
    }

    configuration.UsageHint = hint;
    configuration.UsageHintSource = source;

    return true;
}

std::optional<Pine::TextureUsageHint> Pine::GuessTextureUsageHint(const std::filesystem::path& sourcePath)
{
    // The map type lives in the file name's trailing token, after whatever the texture depicts:
    // 'blood_wall_hell_1_normal'. A plain index is not a token worth reading, so skip past it -
    // 'wall_normal_2' means the same thing as 'wall_normal'.
    const auto fileTokens = Tokenize(sourcePath.stem().string());

    for (auto token = fileTokens.rbegin(); token != fileTokens.rend(); ++token)
    {
        if (IsNumericToken(*token))
        {
            continue;
        }

        if (const auto hint = MatchRule(*token))
        {
            return hint;
        }

        // Only the trailing token is the map type. An earlier one describes the subject, and
        // reading it is how 'metal_floor_5' becomes a metalness map.
        break;
    }

    // Failing that, the directory holding it may say - packs like to sort by map type
    // ('PSX Textures/Normal Maps/'). Only the immediate parent: any higher and we would be
    // reading whichever folder the user happened to unzip the pack into.
    for (const auto& token : Tokenize(sourcePath.parent_path().filename().string()))
    {
        if (const auto hint = MatchRule(token))
        {
            return hint;
        }
    }

    return {};
}

Pine::ByteSpan Pine::Texture2D::SaveAssetData()
{
    TextureSerializer textureSerializer;

    // A texture is kind of special because we have texture data that we also need to save
    // down when saving the texture, and because this is usually not present in CPU memory
    // we'll have to load the asset data, set the changes, and save it again. This is
    // somewhat inefficient but saving assets is not a first-class anyway.
    if (m_ImportData.empty())
    {
        if (!m_FilePath.empty() && std::filesystem::exists(m_FilePath))
        {
            textureSerializer.Read(File::ReadCompressed(m_FilePath));
        }
    }
    else
    {
        // The special case where we're saving this texture the first time post importing.
        for (const auto& importData : m_ImportData)
        {
            TextureMipSerializer textureMipSerializer;

            textureMipSerializer.Width.Write(importData.m_Width);
            textureMipSerializer.Height.Write(importData.m_Height);
            textureMipSerializer.Data.WriteRaw(importData.m_TextureData, importData.m_TextureDataSize);

            textureSerializer.Mips.AddData(textureMipSerializer.Write());

            free(importData.m_TextureData);
        }

        m_ImportData.clear();
    }

    // Save general data
    textureSerializer.Width.Write(m_Width);
    textureSerializer.Height.Write(m_Height);
    textureSerializer.TextureFormat.Write(m_Format);
    textureSerializer.FilteringMode.Write(m_FilteringMode);
    textureSerializer.MipFilteringMode.Write(m_MipFilteringMode);
    textureSerializer.WrapMode.Write(m_WrapMode);
    textureSerializer.CompressionFormat.Write(m_CompressionFormat);
    textureSerializer.AlphaMode.Write(m_AlphaMode);
    textureSerializer.ImportUsageHint.Write(m_ImportConfiguration.UsageHint);
    textureSerializer.ImportUsageHintSource.Write(m_ImportConfiguration.UsageHintSource);
    textureSerializer.ImportCompressionQuality.Write(m_ImportConfiguration.CompressionQuality);
    textureSerializer.ImportGenerateMipMaps.Write(m_ImportConfiguration.GenerateMipmaps);

    return textureSerializer.Write();
}
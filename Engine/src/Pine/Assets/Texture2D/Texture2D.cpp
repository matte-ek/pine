#include "Texture2D.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include <stdexcept>

#ifndef PINE_RUNTIME
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#endif

#include "Importer/TextureImporter.hpp"
#include "Pine/Core/File/File.hpp"
#include "Pine/Threading/Threading.hpp"

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
    textureSerializer.ImportUsageHint.Read(m_ImportConfiguration.UsageHint);
    textureSerializer.ImportGenerateMipMaps.Read(m_ImportConfiguration.GenerateMipmaps);

    m_MipmapLevels = textureSerializer.Data.GetDataCount();

    std::vector<std::shared_ptr<Task>> tasks;

    // Prepare and upload texture data to GPU.
    for (size_t i{}; i < textureSerializer.Data.GetDataCount(); i++)
    {
        auto textureMipDataSpan = textureSerializer.Data.GetData(i);

        tasks.push_back(Threading::QueueTask<void>([textureMipDataSpan, this]()
        {
            // Since it's running on the main thread, this is "thread-safe".
            if (m_Texture == nullptr)
            {
                m_Texture = Graphics::GetGraphicsAPI()->CreateTexture();

                m_Texture->SetFilteringMode(m_FilteringMode);
                m_Texture->SetMipmapFilteringMode(m_MipFilteringMode);
                m_Texture->SetTextureWrapMode(m_WrapMode);
            }

            m_Texture->Bind();

            if (m_CompressionFormat == Graphics::TextureCompressionFormat::Raw)
            {
                m_Texture->UploadTextureData(
                       m_Width,
                       m_Height,
                       m_Format,
                       Graphics::TextureDataFormat::UnsignedByte,
                       textureMipDataSpan.data);
            }
            else
            {
                m_Texture->UploadTextureDataCompressed(
                    m_Width,
                    m_Height,
                    m_Format,
                    m_CompressionFormat,
                    textureMipDataSpan.data,
                    textureMipDataSpan.size);
            }
        },
        TaskThreadingMode::MainThread));
    }

    // Wait for the GPU upload jobs to complete.
    for (const auto& task : tasks)
    {
        Threading::AwaitTaskResult(task);
    }

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
    if (m_TextureData != nullptr)
    {
        stbi_image_free(m_TextureData);
        m_TextureData = nullptr;
    }

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

Pine::ByteSpan Pine::Texture2D::GetTextureData() const
{
    // Call HasTextureData() first!
    assert(m_TextureData != nullptr);

    return {static_cast<std::byte*>(m_TextureData), m_TextureDataSize};
}

Pine::Graphics::TextureFormat Pine::Texture2D::GetFormat() const
{
    return m_Format;
}

Pine::Graphics::TextureCompressionFormat Pine::Texture2D::GetCompressionFormat() const
{
    return m_CompressionFormat;
}

void Pine::Texture2D::SetFilteringMode(Graphics::TextureFilteringMode textureFilteringMode)
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

void Pine::Texture2D::SetMipFilteringMode(Graphics::TextureFilteringMode textureFilteringMode)
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

void Pine::Texture2D::SetWrapMode(Graphics::TextureWrapMode wrapMode)
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

bool Pine::Texture2D::Import()
{
    return Importer::TextureImporter::Import(this);
}

Pine::ByteSpan Pine::Texture2D::SaveAssetData()
{
    TextureSerializer textureSerializer;

    // A texture is kind of special because we have texture data that we also need to save
    // down when saving the texture, and because this is usually not present in CPU memory
    // we'll have to load the asset data, set the changes, and save it again. This is
    // somewhat inefficient but saving assets is not a first-class anyway.
    if (m_TextureData == nullptr)
    {
        if (!m_FilePath.empty() && std::filesystem::exists(m_FilePath))
        {
            textureSerializer.Read(File::ReadCompressed(m_FilePath));
        }
    }
    else
    {
        // The special case where we're saving this texture the first time post importing.
        textureSerializer.Data.AddData(m_TextureData, m_TextureDataSize);

        free(m_TextureData);

        m_TextureData = nullptr;
        m_TextureDataSize = 0;
    }

    // Save general data
    textureSerializer.Width.Write(m_Width);
    textureSerializer.Height.Write(m_Height);
    textureSerializer.TextureFormat.Write(m_Format);
    textureSerializer.FilteringMode.Write(m_FilteringMode);
    textureSerializer.MipFilteringMode.Write(m_MipFilteringMode);
    textureSerializer.WrapMode.Write(m_WrapMode);
    textureSerializer.CompressionFormat.Write(m_CompressionFormat);
    textureSerializer.ImportUsageHint.Write(m_ImportConfiguration.UsageHint);
    textureSerializer.ImportGenerateMipMaps.Write(m_ImportConfiguration.GenerateMipmaps);

    return textureSerializer.Write();
}
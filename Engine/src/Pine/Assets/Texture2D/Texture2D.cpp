#include "Texture2D.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

Pine::Texture2D::Texture2D()
{
    m_Type = AssetType::Texture2D;
    m_LoadMode = AssetLoadMode::MultiThreadPrepare;
}

void Pine::Texture2D::SetStoreTextureData(bool storeTextureData)
{
    m_StoreTextureData = storeTextureData;

    if (!storeTextureData && m_TextureData)
    {
        stbi_image_free(m_TextureData);
        m_TextureData = nullptr;
    }
}

bool Pine::Texture2D::GetStoreTextureData() const
{
    return m_StoreTextureData;
}

bool Pine::Texture2D::PrepareGpuData()
{
    if (m_TextureData != nullptr)
    {
        throw std::runtime_error("Texture2D::PrepareGpuData() called before loading was finished.");
    }

    int width, height, channels;

    const auto data = stbi_load(m_FilePath.string().c_str(), &width, &height, &channels, 4);

    if (data == nullptr)
    {
        stbi_image_free(data);
        return false;
    }

    m_Width = width;
    m_Height = height;

    // BUG: Loading a 3 channel image seems to break stuff right now, so we're just
    // going to force RGBA for now.
    channels = 4;

    switch (channels)
    {
    case 1:
        m_Format = Graphics::TextureFormat::SingleChannel;
        break;
    case 3:
        m_Format = Graphics::TextureFormat::RGB;
        break;
    case 4:
        m_Format = Graphics::TextureFormat::RGBA;
        break;
    default:
        break;
    }

    m_TextureData = data;
    m_State = AssetState::Preparing;

    return true;
}

void Pine::Texture2D::UploadGpuData()
{
    if (m_TextureData == nullptr)
    {
        throw std::runtime_error("Texture2D::UploadGpuData() called before Texture2D::PrepareGpuData()! Texture load failed?");
    }

    m_Texture = Graphics::GetGraphicsAPI()->CreateTexture();

    m_Texture->Bind();
    m_Texture->UploadTextureData(m_Width, m_Height, m_Format, Graphics::TextureDataFormat::UnsignedByte, m_TextureData);

    if (m_GenerateMipmaps)
    {
        m_Texture->GenerateMipmaps();
        m_Texture->SetMipmapFilteringMode(Graphics::TextureFilteringMode::Nearest);
        m_Texture->SetMaxAnisotropy(8);
    }

    // TODO: Use some sort of load-preset option?
    m_Texture->SetFilteringMode(Graphics::TextureFilteringMode::Linear);

    if (!m_StoreTextureData)
    {
        stbi_image_free(m_TextureData);
        m_TextureData = nullptr;
    }
    
    m_State = AssetState::Loaded;
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

void* Pine::Texture2D::GetTextureData() const
{
    return m_TextureData;
}

void Pine::Texture2D::SetGenerateMipmaps(bool value)
{
    m_GenerateMipmaps = value;
}

bool Pine::Texture2D::GetGenerateMipmaps() const
{
    return m_GenerateMipmaps;
}

Pine::Graphics::TextureFormat Pine::Texture2D::GetFormat() const
{
    return m_Format;
}

Pine::Graphics::ITexture* Pine::Texture2D::GetGraphicsTexture() const
{
    return m_Texture;
}

bool Pine::Texture2D::LoadFromFile(AssetLoadStage stage)
{
    switch (stage)
    {
    case AssetLoadStage::Prepare:
        return PrepareGpuData();
    case AssetLoadStage::Finish:
        UploadGpuData();
        return true;
    case AssetLoadStage::Default:
        // If default was used, the user is wants to load the
        // texture in a single-threaded way, so just call both of
        // the load stages manually.

        if (PrepareGpuData())
        {
            UploadGpuData();
            return true;
        }
    }

    return false;
}
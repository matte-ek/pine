#include "Texture2D.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

Pine::Texture2D::Texture2D()
{
    m_Type = AssetType::Texture2D;
    m_LoadMode = AssetLoadMode::MultiThreadPrepare;
}

bool Pine::Texture2D::PrepareGpuData()
{
    if (m_PreparedTextureData != nullptr)
    {
        throw std::runtime_error("Texture2D::PrepareGpuData() called before loading was finished.");
    }

    int width, height, channels;

    const auto data = stbi_load(m_FilePath.string().c_str(), &width, &height, &channels, 0);

    if (data == nullptr)
    {
        stbi_image_free(data);
        return false;
    }

    m_Width = width;
    m_Height = height;

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

    m_PreparedTextureData = data;
    m_State = AssetState::Preparing;

    return true;
}

void Pine::Texture2D::UploadGpuData()
{
    if (m_PreparedTextureData == nullptr)
    {
        throw std::runtime_error("Texture2D::UploadGpuData() called before preparing");
    }

    m_Texture = Graphics::GetGraphicsAPI()->CreateTexture();

    m_Texture->Bind();
    m_Texture->UploadTextureData(m_Width, m_Height, m_Format, Graphics::TextureDataFormat::UnsignedByte, m_PreparedTextureData);

    stbi_image_free(m_PreparedTextureData);

    m_PreparedTextureData = nullptr;
    
    m_State = AssetState::Loaded;
}

void Pine::Texture2D::Dispose()
{
    if (m_PreparedTextureData != nullptr)
    {
        stbi_image_free(m_PreparedTextureData);
        m_PreparedTextureData = nullptr;
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
        if (PrepareGpuData())
        {
            UploadGpuData();
            return true;
        }
    }

    return false;
}
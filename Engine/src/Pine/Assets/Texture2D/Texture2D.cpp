#include "Texture2D.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include <stb_image.h>
#include <stdexcept>

void Pine::Texture2D::PrepareGpuData()
{
    if (m_PreparedGpuData != nullptr)
    {
        throw std::runtime_error("Texture2D::PrepareGpuData() called before loading was finished.");
    }

    int width, height, channels;

    const auto data = stbi_load(m_FilePath.string().c_str(), &width, &height, &channels, 4);

    if (data == nullptr)
    {
        stbi_image_free(data);
        return;
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

    m_PreparedGpuData = data;
    m_GpuAssetLoadState = GPUAssetLoadStage::Upload;
}

void Pine::Texture2D::UploadGpuData()
{
    if (m_PreparedGpuData == nullptr)
    {
        throw std::runtime_error("Texture2D::UploadGpuData() called before preparing");
    }

    m_Texture = Graphics::GetGraphicsAPI()->CreateTexture();

    m_Texture->Bind();
    m_Texture->UploadTextureData(m_Width, m_Height, m_Format, m_PreparedGpuData);

    stbi_image_free(m_PreparedGpuData);

    m_PreparedGpuData = nullptr;

    m_GpuAssetLoadState = GPUAssetLoadStage::Ready;
    m_State = AssetState::Loaded;
}

void Pine::Texture2D::Dispose()
{
    if (m_PreparedGpuData != nullptr)
    {
        stbi_image_free(m_PreparedGpuData);
        m_PreparedGpuData = nullptr;
    }
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

#include "GLTexture.hpp"
#include "Pine/Core/Log/Log.hpp"

#include <GL/glew.h>
#include <stdexcept>

namespace
{

    std::uint32_t m_ActiveTexture = 10000;
    std::uint32_t m_BoundTextures[64] = { 10000 };

    struct GLTextureFormat
    {
        int openglFormat = 0;
        int openglInternalFormat = 0;
    };

    std::uint32_t TranslateTextureType(Pine::Graphics::TextureType type)
    {
        switch (type)
        {
        case Pine::Graphics::TextureType::Texture2D:
            return GL_TEXTURE_2D;
        case Pine::Graphics::TextureType::CubeMap:
            return GL_TEXTURE_CUBE_MAP;
        default:
            throw std::runtime_error("Unsupported texture type.");
        }
    }

    std::uint32_t TranslateTextureDataFormatType(Pine::Graphics::TextureDataFormat type)
    {
        switch (type)
        {
        case Pine::Graphics::TextureDataFormat::UnsignedByte:
            return GL_UNSIGNED_BYTE;
        case Pine::Graphics::TextureDataFormat::Float:
            return GL_FLOAT;
        default:
            throw std::runtime_error("Unsupported texture data format type.");
        }
    }

    GLTextureFormat TranslateOpenGLTextureFormat(Pine::Graphics::TextureFormat format)
    {
        int openglFormat;
        int openglInternalFormat = 0;

        switch (format)
        {
            case Pine::Graphics::TextureFormat::SingleChannel:
                openglFormat = GL_R8;
                break;
            case Pine::Graphics::TextureFormat::RGB:
                openglFormat = GL_RGB;
                break;
            case Pine::Graphics::TextureFormat::RGBA:
                openglFormat = GL_RGBA;
                break;
            case Pine::Graphics::TextureFormat::RGB16F:
                openglFormat = GL_RGB16F;
                openglInternalFormat = GL_RGB;
                break;
            case Pine::Graphics::TextureFormat::RGBA16F:
                openglFormat = GL_RGBA16F;
                openglInternalFormat = GL_RGBA;
                break;
            case Pine::Graphics::TextureFormat::Depth:
                openglFormat = GL_DEPTH_COMPONENT;
                break;
            case Pine::Graphics::TextureFormat::Alpha:
                openglFormat = GL_ALPHA;
                break;
            default:
                throw std::runtime_error("Unsupported texture format.");
        }

        if (openglInternalFormat == 0)
            openglInternalFormat = openglFormat;

        return {openglFormat, openglInternalFormat};
    }

}

Pine::Graphics::GLTexture::GLTexture()
{
    glGenTextures(1, &m_Id);
}

void Pine::Graphics::GLTexture::Bind(int textureIndex)
{
    if (m_BoundTextures[textureIndex] == textureIndex)
    {
        return;
    }

    if (m_ActiveTexture != textureIndex)
    {
        glActiveTexture(GL_TEXTURE0 + textureIndex);

        m_ActiveTexture = textureIndex;
    }

    glBindTexture(TranslateTextureType(m_Type), m_Id);

    m_BoundTextures[textureIndex] = m_Id;
}

void Pine::Graphics::GLTexture::CopyTextureData(ITexture*texture,
                                                TextureUploadTarget textureUploadTarget,
                                                Vector4i srcRect,
                                                Vector2i dstPos)
{
    if (srcRect.x < 0)
        srcRect = Vector4i(0, 0, texture->GetWidth(), texture->GetHeight());

    auto textureType = TranslateTextureType(m_Type);
    auto cubeMapTextureType = m_Type == TextureType::CubeMap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<int>(textureUploadTarget) - 1 : GL_TEXTURE_2D;
    auto [openglFormat, openglInternalFormat] = TranslateOpenGLTextureFormat(texture->GetTextureFormat());

    // Since glCopyImageSubData only copies the data, we'll need to manually allocate it first.
    glTexImage2D(cubeMapTextureType,
                 0,
                 openglInternalFormat,
                 texture->GetWidth(),
                 texture->GetHeight(),
                 0,
                 openglFormat,
                 TranslateTextureDataFormatType(texture->GetTextureDataFormat()),
                 nullptr);

    // We have to set the filtering properties before copying the data
    glTexParameteri(textureType, GL_TEXTURE_MIN_FILTER, texture->GetFilteringMode() == TextureFilteringMode::Linear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(textureType, GL_TEXTURE_MAG_FILTER, texture->GetFilteringMode() == TextureFilteringMode::Linear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(textureType, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(textureType, GL_TEXTURE_WRAP_T, GL_REPEAT);

    auto srcId = *reinterpret_cast<std::uint32_t*>(texture->GetGraphicsIdentifier());

    // TODO: This doesn't currently work for cube maps, for whatever reason.
    glCopyImageSubData(srcId,
                       GL_TEXTURE_2D,
                       0,
                       0, 0,
                       0,
                       m_Id,
                       textureType,
                       0,
                       0, 0,
                       m_Type == TextureType::CubeMap ? static_cast<int>(textureUploadTarget) - 1 : 0,
                       srcRect.z,
                       srcRect.w,
                       1);
}

void Pine::Graphics::GLTexture::Dispose()
{
    glDeleteTextures(1, &m_Id);
}

void Pine::Graphics::GLTexture::UploadTextureData(int width, int height, TextureFormat format, TextureDataFormat dataFormat, void* data)
{
    auto [openglFormat, openglInternalFormat] = TranslateOpenGLTextureFormat(format);

    const auto openglType = TranslateTextureType(m_Type);

    glTexImage2D(openglType, 0, openglInternalFormat, width, height, 0, openglFormat, TranslateTextureDataFormatType(dataFormat), data);

    glTexParameteri(openglType, GL_TEXTURE_MIN_FILTER, m_FilteringMode == TextureFilteringMode::Linear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(openglType, GL_TEXTURE_MAG_FILTER, m_FilteringMode == TextureFilteringMode::Linear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(openglType, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(openglType, GL_TEXTURE_WRAP_T, GL_REPEAT);

    m_Width = width;
    m_Height = height;
    m_TextureFormat = format;
    m_TextureDataFormat = dataFormat;
}

Pine::Graphics::TextureType Pine::Graphics::GLTexture::GetType()
{
    return m_Type;
}

void Pine::Graphics::GLTexture::SetType(TextureType type)
{
    m_Type = type;
}

void Pine::Graphics::GLTexture::SetFilteringMode(TextureFilteringMode mode)
{
    m_FilteringMode = mode;

    if (m_Id != 0)
    {
        const auto openglType = TranslateTextureType(m_Type);

        Bind();

        glTexParameteri(openglType, GL_TEXTURE_MIN_FILTER, m_FilteringMode == TextureFilteringMode::Linear ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(openglType, GL_TEXTURE_MAG_FILTER, m_FilteringMode == TextureFilteringMode::Linear ? GL_LINEAR : GL_NEAREST);
    }
}

Pine::Graphics::TextureFilteringMode Pine::Graphics::GLTexture::GetFilteringMode()
{
    return m_FilteringMode;
}

std::uint32_t Pine::Graphics::GLTexture::GetId() const
{
    return m_Id;
}

void* Pine::Graphics::GLTexture::GetGraphicsIdentifier()
{
    return &m_Id;
}

int Pine::Graphics::GLTexture::GetWidth()
{
    return m_Width;
}

int Pine::Graphics::GLTexture::GetHeight()
{
    return m_Height;
}

Pine::Graphics::TextureFormat Pine::Graphics::GLTexture::GetTextureFormat()
{
    return m_TextureFormat;
}

Pine::Graphics::TextureDataFormat Pine::Graphics::GLTexture::GetTextureDataFormat()
{
    return m_TextureDataFormat;
}

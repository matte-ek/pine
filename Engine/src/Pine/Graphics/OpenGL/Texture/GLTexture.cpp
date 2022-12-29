#include "GLTexture.hpp"
#include "Pine/Core/Log/Log.hpp"

#include <GL/glew.h>
#include <stdexcept>

namespace
{

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

}

Pine::Graphics::GLTexture::GLTexture()
{
    glGenTextures(1, &m_Id);
}

void Pine::Graphics::GLTexture::Bind()
{
    glBindTexture(TranslateTextureType(m_Type), m_Id);
}

void Pine::Graphics::GLTexture::Dispose()
{
    glDeleteTextures(1, &m_Id);
}

void Pine::Graphics::GLTexture::UploadTextureData(int width, int height, TextureFormat format, TextureDataFormat dataFormat, void* data)
{
    int openglFormat;
    int openglInternalFormat = 0;

    switch (format)
    {
    case TextureFormat::SingleChannel:
        openglFormat = GL_R8;
        break;
    case TextureFormat::RGB:
        openglFormat = GL_RGB;
        break;
    case TextureFormat::RGBA:
        openglFormat = GL_RGBA;
        break;
    case TextureFormat::RGB16F:
        openglFormat = GL_RGB16F;
        openglInternalFormat = GL_RGB;
        break;
    case TextureFormat::RGBA16F:
        openglFormat = GL_RGBA16F;
        openglInternalFormat = GL_RGBA;
        break;
    case TextureFormat::Depth:
        openglFormat = GL_DEPTH_COMPONENT;
        break;
    default:
        throw std::runtime_error("Unsupported texture format.");
    }

    if (openglInternalFormat == 0)
        openglInternalFormat = openglFormat;

    const auto openglType = TranslateTextureType(m_Type);

    glTexImage2D(openglType, 0, openglInternalFormat, width, height, 0, openglFormat, TranslateTextureDataFormatType(dataFormat), data);

    // TODO: This shouldn't be hard coded.

    glTexParameteri(openglType, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(openglType, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(openglType, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(openglType, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

Pine::Graphics::TextureType Pine::Graphics::GLTexture::GetType()
{
    return m_Type;
}

void Pine::Graphics::GLTexture::SetType(Pine::Graphics::TextureType type)
{
    m_Type = type;
}

std::uint32_t Pine::Graphics::GLTexture::GetId() const
{
    return m_Id;
}

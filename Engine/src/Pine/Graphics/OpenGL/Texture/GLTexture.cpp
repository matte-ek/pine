#include "GLTexture.hpp"

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
        case Pine::Graphics::TextureType::Cubemap:
            return GL_TEXTURE_CUBE_MAP;
        default:
            throw std::runtime_error("Unsupported texture type.");
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

void Pine::Graphics::GLTexture::UploadTextureData(int width, int height, TextureFormat format, void* data)
{
    int openglFormat;

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
    default:
        throw std::runtime_error("Unsupported texture format.");
    }

    const auto openglType = TranslateTextureType(m_Type);

    glTexImage2D(openglType, 0, openglFormat, width, height, 0, openglFormat, GL_UNSIGNED_BYTE, data);

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

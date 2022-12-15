#include "GLTexture.hpp"

#include <GL/glew.h>
#include <stdexcept>

Pine::Graphics::GLTexture::GLTexture()
{
    glGenTextures(1, &m_Id);
}

void Pine::Graphics::GLTexture::Bind()
{
    glBindTexture(GL_TEXTURE_2D, m_Id);
}

void Pine::Graphics::GLTexture::Dispose()
{
    glDeleteTextures(1, &m_Id);
}

void Pine::Graphics::GLTexture::UploadTextureData(int width, int height, TextureFormat format, void* data)
{
    int openglFormat = 0;

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

    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, openglFormat, GL_FLOAT, data );

    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
}
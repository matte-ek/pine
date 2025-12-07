#include "GLTexture.hpp"
#include "Pine/Core/Log/Log.hpp"

#include <GL/glew.h>
#include <stdexcept>

namespace
{

    std::uint32_t m_ActiveTexture = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t m_BoundTextures[64] = {std::numeric_limits<std::uint32_t>::max() };

    struct GLTextureFormat
    {
        int openglFormat = 0;
        int openglInternalFormat = 0;
    };

    std::uint32_t TranslateTextureType(Pine::Graphics::TextureType type, bool multiSample)
    {
        switch (type)
        {
            case Pine::Graphics::TextureType::Texture2D:
                return multiSample ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
            case Pine::Graphics::TextureType::Texture2DArray:
                return GL_TEXTURE_2D_ARRAY;
            case Pine::Graphics::TextureType::CubeMap:
                return GL_TEXTURE_CUBE_MAP;
            default:
                throw std::runtime_error("Unsupported texture type.");
        }
    }

    std::uint32_t TranslateWrapMode(Pine::Graphics::TextureWrapMode mode)
    {
        switch (mode)
        {
            case Pine::Graphics::TextureWrapMode::Repeat:
                return GL_REPEAT;
            case Pine::Graphics::TextureWrapMode::ClampToBorder:
                return GL_CLAMP_TO_BORDER;
            case Pine::Graphics::TextureWrapMode::ClampToEdge:
                return GL_CLAMP_TO_EDGE;
            default:
                throw std::runtime_error("Unsupported wrap type.");
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
            case Pine::Graphics::TextureDataFormat::UnsignedInt24_8:
                return GL_UNSIGNED_INT_24_8;
            default:
                throw std::runtime_error("Unsupported texture data format type.");
        }
    }

    std::int32_t TranslateSwizzleMaskChannel(Pine::Graphics::SwizzleMaskChannel channel)
    {
        switch (channel)
        {
            case Pine::Graphics::SwizzleMaskChannel::Red:
                return GL_RED;
            case Pine::Graphics::SwizzleMaskChannel::Green:
                return GL_GREEN;
            case Pine::Graphics::SwizzleMaskChannel::Blue:
                return GL_BLUE;
            case Pine::Graphics::SwizzleMaskChannel::Alpha:
                return GL_ALPHA;
            case Pine::Graphics::SwizzleMaskChannel::Zero:
                return GL_ZERO;
            case Pine::Graphics::SwizzleMaskChannel::One:
                return GL_ONE;
            default:
                throw std::runtime_error("Unsupported swizzle mask channel.");
        }
    }

    GLTextureFormat TranslateOpenGLTextureFormat(Pine::Graphics::TextureFormat format)
    {
        int openglFormat;
        int openglInternalFormat = 0;

        switch (format)
        {
            case Pine::Graphics::TextureFormat::SingleChannel:
                openglFormat = GL_RED;
                openglInternalFormat = GL_R8;
                break;
            case Pine::Graphics::TextureFormat::RGB:
                openglFormat = GL_RGB;
                break;
            case Pine::Graphics::TextureFormat::RGBA:
                openglFormat = GL_RGBA;
                break;
            case Pine::Graphics::TextureFormat::RGB16F:
                openglFormat = GL_RGB;
                openglInternalFormat = GL_RGBA16F;
                break;
            case Pine::Graphics::TextureFormat::RGBA16F:
                openglFormat = GL_RGBA;
                openglInternalFormat = GL_RGBA16F;
                break;
            case Pine::Graphics::TextureFormat::Depth:
                openglFormat = GL_DEPTH_COMPONENT;
                break;
            case Pine::Graphics::TextureFormat::DepthStencil:
                openglFormat = GL_DEPTH_STENCIL;
                openglInternalFormat = GL_DEPTH24_STENCIL8;
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

    glBindTexture(TranslateTextureType(m_Type, m_IsMultiSampled), m_Id);

    m_BoundTextures[textureIndex] = m_Id;
}

void Pine::Graphics::GLTexture::CopyTextureData(ITexture *texture,
                                                TextureUploadTarget textureUploadTarget,
                                                Vector4i srcRect,
                                                Vector2i dstPos)
{
    if (srcRect.x < 0)
        srcRect = Vector4i(0, 0, texture->GetWidth(), texture->GetHeight());

    auto textureType = TranslateTextureType(m_Type, m_IsMultiSampled);
    auto cubeMapTextureType = m_Type == TextureType::CubeMap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<int>(textureUploadTarget) - 1 : GL_TEXTURE_2D;
    auto [openglFormat, openglInternalFormat] = TranslateOpenGLTextureFormat(texture->GetTextureFormat());

    // Since glCopyImageSubData only copies the data, we'll need to manually allocate it first.
    // EDIT: Since I cannot get glCopyImageSubData working, we'll copy data to the CPU, then to the GPU again :S

    auto srcId = *reinterpret_cast<std::uint32_t *>(texture->GetGraphicsIdentifier());

    size_t bufferSize = texture->GetWidth() * texture->GetHeight() * (texture->GetTextureFormat() == TextureFormat::RGBA ? 4 : 3);
    void *buffer = malloc(bufferSize);

    glGetTextureImage(static_cast<int>(srcId), 0, openglFormat, GL_UNSIGNED_BYTE, bufferSize, buffer);

    glTexImage2D(cubeMapTextureType,
                 0,
                 openglInternalFormat,
                 texture->GetWidth(),
                 texture->GetHeight(),
                 0,
                 openglFormat,
                 TranslateTextureDataFormatType(texture->GetTextureDataFormat()),
                 buffer);

    free(buffer);

    // We have to set the filtering properties before copying the data
    glTexParameteri(textureType, GL_TEXTURE_MIN_FILTER, texture->GetFilteringMode() == TextureFilteringMode::Linear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(textureType, GL_TEXTURE_MAG_FILTER, texture->GetFilteringMode() == TextureFilteringMode::Linear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(textureType, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(textureType, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // TODO: This doesn't currently work for cube maps, for whatever reason.
    /*glCopyImageSubData(srcId,
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
    */
}

void Pine::Graphics::GLTexture::Dispose()
{
    glDeleteTextures(1, &m_Id);
}

void Pine::Graphics::GLTexture::UploadTextureData(int width, int height, TextureFormat format, TextureDataFormat dataFormat, void *data)
{
    auto [openglFormat, openglInternalFormat] = TranslateOpenGLTextureFormat(format);

    const auto openglType = TranslateTextureType(m_Type, m_IsMultiSampled);

    if (m_Type != TextureType::Texture2DArray)
    {
        if (m_IsMultiSampled)
            glTexImage2DMultisample(openglType, m_Samples, openglInternalFormat, width, height, GL_TRUE);
        else
            glTexImage2D(openglType, 0, openglInternalFormat, width, height, 0, openglFormat, TranslateTextureDataFormatType(dataFormat), data);
    }
    else
    {
        // You need to set the array size beforehand using SetArraySize().
        assert(m_ArraySize != -1);

        glTexImage3D(openglType, 0, openglInternalFormat, width, height, m_ArraySize, 0, openglFormat, TranslateTextureDataFormatType(dataFormat), data);
    }

    if (!m_IsMultiSampled)
    {
        glTexParameteri(openglType, GL_TEXTURE_MIN_FILTER, m_FilteringMode == TextureFilteringMode::Linear ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(openglType, GL_TEXTURE_MAG_FILTER, m_FilteringMode == TextureFilteringMode::Linear ? GL_LINEAR : GL_NEAREST);

        glTexParameteri(openglType, GL_TEXTURE_WRAP_S, TranslateWrapMode(m_WrapMode));
        glTexParameteri(openglType, GL_TEXTURE_WRAP_T, TranslateWrapMode(m_WrapMode));

        glTexParameterfv(openglType, GL_TEXTURE_BORDER_COLOR, &m_BorderColor[0]);
    }

    if (m_HasCustomSwizzleMask)
    {
        UpdateSwizzleMask();
    }

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

    UpdateTextureFiltering();
}

Pine::Graphics::TextureFilteringMode Pine::Graphics::GLTexture::GetFilteringMode()
{
    return m_FilteringMode;
}

void Pine::Graphics::GLTexture::SetMipmapFilteringMode(TextureFilteringMode mode)
{
    m_MipmapFilteringMode = mode;

    UpdateTextureFiltering();
}

Pine::Graphics::TextureFilteringMode Pine::Graphics::GLTexture::GetMipmapFilteringMode()
{
    return m_MipmapFilteringMode;
}

void Pine::Graphics::GLTexture::SetTextureWrapMode(TextureWrapMode mode)
{
    m_WrapMode = mode;
}

Pine::Graphics::TextureWrapMode Pine::Graphics::GLTexture::GetTextureWrapMode()
{
    return m_WrapMode;
}

void Pine::Graphics::GLTexture::SetBorderColor(Vector4f color)
{
    m_BorderColor = color;

    glTexParameterfv(TranslateTextureType(m_Type, m_IsMultiSampled), GL_TEXTURE_BORDER_COLOR, &m_BorderColor[0]);
}

Pine::Vector4f Pine::Graphics::GLTexture::GetBorderColor()
{
    return m_BorderColor;
}

void Pine::Graphics::GLTexture::SetCompareModeLowerEqual()
{
    // TODO: Allow these to be configured?
    glTexParameteri(TranslateTextureType(m_Type, m_IsMultiSampled), GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(TranslateTextureType(m_Type, m_IsMultiSampled), GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
}

void Pine::Graphics::GLTexture::SetMaxAnisotropy(const float value)
{
    glTexParameterf(TranslateTextureType(m_Type, m_IsMultiSampled), GL_TEXTURE_MAX_ANISOTROPY_EXT, value);
}

std::uint32_t Pine::Graphics::GLTexture::GetId() const
{
    return m_Id;
}

void *Pine::Graphics::GLTexture::GetGraphicsIdentifier()
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

void Pine::Graphics::GLTexture::GenerateMipmaps()
{
    m_HasMipmaps = true;

    glGenerateMipmap(TranslateTextureType(m_Type, m_IsMultiSampled));
}

void Pine::Graphics::GLTexture::UpdateTextureFiltering()
{
    if (m_Id == 0)
    {
        return;
    }

    const auto openglType = TranslateTextureType(m_Type, m_IsMultiSampled);

    Bind();

    int minFilter = m_FilteringMode == TextureFilteringMode::Linear ? GL_LINEAR : GL_NEAREST;
    int magFilter = m_FilteringMode == TextureFilteringMode::Linear ? GL_LINEAR : GL_NEAREST;

    if (m_HasMipmaps)
    {
        if (m_MipmapFilteringMode == TextureFilteringMode::Nearest)
            minFilter = m_FilteringMode == TextureFilteringMode::Linear ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST;
        else
            minFilter = m_FilteringMode == TextureFilteringMode::Linear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR;
    }

    glTexParameteri(openglType, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(openglType, GL_TEXTURE_MAG_FILTER, magFilter);
}

void Pine::Graphics::GLTexture::SetMultiSampled(bool multiSampled)
{
    m_IsMultiSampled = multiSampled;
}

bool Pine::Graphics::GLTexture::IsMultiSampled()
{
    return m_IsMultiSampled;
}

void Pine::Graphics::GLTexture::SetSamples(int samples)
{
    m_Samples = samples;
}

int Pine::Graphics::GLTexture::GetSamples()
{
    return m_Samples;
}

void Pine::Graphics::GLTexture::SetArraySize(int arraySize)
{
    m_ArraySize = arraySize;
}

int Pine::Graphics::GLTexture::GetArraySize()
{
    return m_ArraySize;
}

bool Pine::Graphics::GLTexture::HasCustomSwizzleMask()
{
    return m_HasCustomSwizzleMask;
}

void Pine::Graphics::GLTexture::SetSwizzleMask(Pine::Graphics::SwizzleMaskChannel r, Pine::Graphics::SwizzleMaskChannel g, Pine::Graphics::SwizzleMaskChannel b, Pine::Graphics::SwizzleMaskChannel a)
{
    m_SwizzleMask = { r, g, b, a };
    m_HasCustomSwizzleMask = true;

    if (m_Id != 0)
    {
        UpdateSwizzleMask();
    }
}

void Pine::Graphics::GLTexture::ResetSwizzleMask()
{
    m_SwizzleMask = {SwizzleMaskChannel::Red, SwizzleMaskChannel::Green, SwizzleMaskChannel::Blue, SwizzleMaskChannel::Alpha};
    m_HasCustomSwizzleMask = false;

    if (m_Id != 0)
    {
        UpdateSwizzleMask();
    }
}

void Pine::Graphics::GLTexture::UpdateSwizzleMask() const
{
    glTexParameteri(TranslateTextureType(m_Type, m_IsMultiSampled), GL_TEXTURE_SWIZZLE_R, TranslateSwizzleMaskChannel(m_SwizzleMask[0]));
    glTexParameteri(TranslateTextureType(m_Type, m_IsMultiSampled), GL_TEXTURE_SWIZZLE_G, TranslateSwizzleMaskChannel(m_SwizzleMask[1]));
    glTexParameteri(TranslateTextureType(m_Type, m_IsMultiSampled), GL_TEXTURE_SWIZZLE_B, TranslateSwizzleMaskChannel(m_SwizzleMask[2]));
    glTexParameteri(TranslateTextureType(m_Type, m_IsMultiSampled), GL_TEXTURE_SWIZZLE_A, TranslateSwizzleMaskChannel(m_SwizzleMask[3]));
}

void Pine::Graphics::GLTexture::UpdateWrapMode()
{
    if (m_Id == 0)
    {
        return;
    }

    const auto openglType = TranslateTextureType(m_Type, m_IsMultiSampled);

    Bind();

    glTexParameteri(openglType, GL_TEXTURE_WRAP_S, TranslateWrapMode(m_WrapMode));
    glTexParameteri(openglType, GL_TEXTURE_WRAP_T, TranslateWrapMode(m_WrapMode));
}

void Pine::Graphics::GLTexture::ResetChangeTracking()
{
    for (unsigned int & boundTexture : m_BoundTextures)
    {
        boundTexture = std::numeric_limits<std::uint32_t>::max();
    }

    m_ActiveTexture = std::numeric_limits<std::uint32_t>::max();
}

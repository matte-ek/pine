#include "GLFrameBuffer.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Graphics/OpenGL/Texture/GLTexture.hpp"
#include <GL/glew.h>
#include <vector>

namespace
{

    inline std::uint32_t TranslateReadFormat(Pine::Graphics::ReadFormat format)
    {
        switch (format)
        {
        case Pine::Graphics::ReadFormat::Depth:
            return GL_DEPTH_COMPONENT;
        case Pine::Graphics::ReadFormat::Stencil:
            return GL_STENCIL_INDEX;
        case Pine::Graphics::ReadFormat::RGBA:
            return GL_RGBA;
        case Pine::Graphics::ReadFormat::RGB:
            return GL_RGB;
        case Pine::Graphics::ReadFormat::RG:
            return GL_RG;
        case Pine::Graphics::ReadFormat::R:
            return GL_RED;
        default:
            return GL_RGBA;
        }
    }

    inline std::uint32_t TranslateTextureDataFormatType(Pine::Graphics::TextureDataFormat type)
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

    inline std::uint32_t TranslateBufferType(Pine::Graphics::Buffers buffer)
    {
        switch (buffer)
        {
        case Pine::Graphics::ColorBuffer:
            return GL_COLOR_BUFFER_BIT;
        case Pine::Graphics::DepthBuffer:
            return GL_DEPTH_BUFFER_BIT;
        case Pine::Graphics::StencilBuffer:
            return GL_STENCIL_BUFFER_BIT;
        default:
            throw std::runtime_error("Unsupported buffer type.");
        }
    }

    inline std::uint32_t TranslateAttachmentType(Pine::Graphics::BufferAttachment attachment)
    {
        switch (attachment)
        {
            case Pine::Graphics::BufferAttachment::Color:
                return GL_COLOR_ATTACHMENT0;
            case Pine::Graphics::BufferAttachment::Depth:
                return GL_DEPTH_ATTACHMENT;
            case Pine::Graphics::BufferAttachment::DepthStencil:
                return GL_DEPTH_STENCIL_ATTACHMENT;
            default:
                throw std::runtime_error("Unsupported attachment type.");
        }
    }
}

Pine::Vector2i Pine::Graphics::GLFrameBuffer::GetSize()
{
    return m_Size;
}

Pine::Graphics::ITexture* Pine::Graphics::GLFrameBuffer::GetColorBuffer()
{
    return m_ColorBuffer;
}

Pine::Graphics::ITexture* Pine::Graphics::GLFrameBuffer::GetDepthBuffer()
{
    return m_DepthBuffer;
}

Pine::Graphics::ITexture* Pine::Graphics::GLFrameBuffer::GetNormalBuffer()
{
    return m_NormalBuffer;
}

Pine::Graphics::ITexture* Pine::Graphics::GLFrameBuffer::GetDepthStencilBuffer()
{
    return m_DepthStencilBuffer;
}

void Pine::Graphics::GLFrameBuffer::Bind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_Id);
}

void Pine::Graphics::GLFrameBuffer::Dispose()
{
    glDeleteFramebuffers(1, &m_Id);

    if (m_ColorBuffer != nullptr)
    {
        m_ColorBuffer->Dispose();

        delete m_ColorBuffer;
    }

    if (m_DepthBuffer != nullptr)
    {
        m_DepthBuffer->Dispose();

        delete m_DepthBuffer;
    }

    if (m_NormalBuffer != nullptr)
    {
        m_NormalBuffer->Dispose();

        delete m_NormalBuffer;
    }

    m_Id = 0;
}

void Pine::Graphics::GLFrameBuffer::Prepare()
{
    // Please dispose of the previous frame buffer before creating a new one.
    assert(m_Id == 0);

    m_AttachedDrawBuffers.clear();

    glGenFramebuffers(1, &m_Id);
    glBindFramebuffer(GL_FRAMEBUFFER, m_Id);
}

void Pine::Graphics::GLFrameBuffer::AttachTextures(int width, int height, int buffers, int multiSample)
{
    if (buffers & StencilBuffer)
    {
        // Notice: When creating a stencil buffer, a depth buffer must also be created.
        assert(buffers & DepthBuffer);
    }

    const bool multiSampleEnabled = multiSample != 0;
    const auto textureType = multiSampleEnabled ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;

    if (buffers & ColorBuffer)
    {
        m_ColorBuffer = new GLTexture();

        m_ColorBuffer->SetMultiSampled(multiSampleEnabled);
        m_ColorBuffer->SetSamples(multiSample);

        m_ColorBuffer->Bind();
        m_ColorBuffer->UploadTextureData(width, height, TextureFormat::RGBA, TextureDataFormat::UnsignedByte, nullptr);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureType, m_ColorBuffer->GetId(), 0);

        m_AttachedDrawBuffers.push_back(GL_COLOR_ATTACHMENT0);
    }

    if (buffers & NormalBuffer)
    {
        m_NormalBuffer = new GLTexture();
        m_NormalBuffer->Bind();
        m_NormalBuffer->UploadTextureData(width, height, TextureFormat::RGBA16F, TextureDataFormat::Float, nullptr);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, textureType, m_NormalBuffer->GetId(), 0);

        m_AttachedDrawBuffers.push_back(GL_COLOR_ATTACHMENT1);
    }

    if (buffers & StencilBuffer)
    {
        m_DepthStencilBuffer = new GLTexture();

        m_DepthStencilBuffer->SetMultiSampled(multiSampleEnabled);
        m_DepthStencilBuffer->SetSamples(multiSample);

        m_DepthStencilBuffer->Bind();
        m_DepthStencilBuffer->UploadTextureData(width, height, TextureFormat::DepthStencil, TextureDataFormat::UnsignedInt24_8, nullptr);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, textureType, m_DepthStencilBuffer->GetId(), 0);
    }
    else
    {
        if (buffers & DepthBuffer)
        {
            m_DepthBuffer = new GLTexture();

            m_DepthBuffer->SetMultiSampled(multiSampleEnabled);
            m_DepthBuffer->SetSamples(multiSample);

            m_DepthBuffer->Bind();
            m_DepthBuffer->UploadTextureData(width, height, TextureFormat::Depth, TextureDataFormat::Float, nullptr);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, textureType, m_DepthBuffer->GetId(), 0);
        }
    }

    m_Size = Vector2i(width, height);
}

void Pine::Graphics::GLFrameBuffer::AttachTexture(ITexture *texture, BufferAttachment attachment, int attachmentOffset)
{
    const auto glTexture = dynamic_cast<GLTexture*>(texture);
    const bool isArrayTexture = texture->GetArraySize() > 0;

    assert(glTexture != nullptr);

    if (isArrayTexture)
        glFramebufferTexture(GL_FRAMEBUFFER, TranslateAttachmentType(attachment) + attachmentOffset, glTexture->GetId(), 0);
    else
        glFramebufferTexture2D(GL_FRAMEBUFFER, TranslateAttachmentType(attachment) + attachmentOffset, texture->IsMultiSampled() ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, glTexture->GetId(), 0);

    if (attachment == BufferAttachment::Color)
    {
        m_AttachedDrawBuffers.push_back(TranslateAttachmentType(attachment) + attachmentOffset);
    }

    // TODO: This kind of sucks, since we can technically have multiple color buffers for example,
    // and right now we just hardcode the second color buffer as "normal" buffer.

    switch (attachment)
    {
        case BufferAttachment::Color:
            if (attachmentOffset == 0)
                m_ColorBuffer = glTexture;
            else
                m_NormalBuffer = glTexture;
            break;
        case BufferAttachment::Depth:
            m_DepthBuffer = glTexture;
            break;
        case BufferAttachment::DepthStencil:
            m_DepthStencilBuffer = glTexture;
            break;
        default:
            break;
    }
}

bool Pine::Graphics::GLFrameBuffer::Finish()
{
    // Tell OpenGL about the newly created draw attachments
    if (!m_AttachedDrawBuffers.empty())
    {
        glDrawBuffers(static_cast<std::int32_t>(m_AttachedDrawBuffers.size()), m_AttachedDrawBuffers.data());
    }
    else
    {
        // Tell OpenGL we don't have any draw buffer.
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }

    // Make sure the frame buffer is valid
    const auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        Log::Error("Failure in creating frame buffer: " + std::to_string(status));
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
}

void Pine::Graphics::GLFrameBuffer::Blit(IFrameBuffer *source, Buffers buffer, Vector4i srcRect, Vector4i dstRect)
{
    if (srcRect == Vector4i(-1))
        srcRect = Vector4i(0, 0, source->GetSize().x, source->GetSize().y);
    if (dstRect == Vector4i(-1))
        dstRect = Vector4i(0, 0, m_Size.x, m_Size.y);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, dynamic_cast<GLFrameBuffer*>(source)->m_Id);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_Id);

    glBlitFramebuffer(srcRect.x, srcRect.y, srcRect.z, srcRect.w, dstRect.x, dstRect.y, dstRect.z, dstRect.w, TranslateBufferType(buffer), GL_NEAREST);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void Pine::Graphics::GLFrameBuffer::ReadPixels(Pine::Vector2i position, Pine::Vector2i size, Pine::Graphics::ReadFormat readFormat, Pine::Graphics::TextureDataFormat dataFormat, size_t bufferSize, void *buffer)
{
    glReadPixels(position.x, position.y, size.x, size.y, TranslateReadFormat(readFormat), TranslateTextureDataFormatType(dataFormat), buffer);
}
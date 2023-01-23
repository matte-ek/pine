#include "GLFrameBuffer.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Graphics/OpenGL/Texture/GLTexture.hpp"
#include <GL/glew.h>
#include <vector>

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
}

bool Pine::Graphics::GLFrameBuffer::Create(int width, int height, std::uint32_t buffers)
{
    // Create the frame buffer object itself
    glGenFramebuffers(1, &m_Id);
    glBindFramebuffer(GL_FRAMEBUFFER, m_Id);

    // Attach specified buffers to the frame buffer

    std::vector<std::uint32_t> attachedDrawBuffers;

    if (buffers & Buffers::ColorBuffer)
    {
        m_ColorBuffer = new GLTexture();

        m_ColorBuffer->Bind();
        m_ColorBuffer->UploadTextureData(width, height, TextureFormat::RGBA, TextureDataFormat::UnsignedByte, nullptr);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ColorBuffer->GetId(), 0);

        attachedDrawBuffers.push_back(GL_COLOR_ATTACHMENT0);
    }

    if (buffers & Buffers::NormalBuffer)
    {
        m_NormalBuffer = new GLTexture();

        m_NormalBuffer->Bind();
        m_NormalBuffer->UploadTextureData(width, height, TextureFormat::RGBA16F, TextureDataFormat::Float, nullptr);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_NormalBuffer->GetId(), 0);

        attachedDrawBuffers.push_back(GL_COLOR_ATTACHMENT1);
    }

    if (buffers & Buffers::DepthBuffer)
    {
        m_DepthBuffer = new GLTexture();

        m_DepthBuffer->Bind();
        m_DepthBuffer->UploadTextureData(width, height, TextureFormat::Depth, TextureDataFormat::Float, nullptr);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_DepthBuffer->GetId(), 0);
    }

    // Tell OpenGL about the newly created draw attachments

    if (!attachedDrawBuffers.empty())
    {
        glDrawBuffers(static_cast<std::int32_t>(attachedDrawBuffers.size()), attachedDrawBuffers.data());
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

    m_Size = Vector2i(width, height);

    return true;
}


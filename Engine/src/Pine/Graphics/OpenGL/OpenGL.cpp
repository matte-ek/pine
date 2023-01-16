#include "OpenGL.hpp"
#include "Pine/Graphics/OpenGL/FrameBuffer/GLFrameBuffer.hpp"
#include "Pine/Graphics/OpenGL/ShaderProgram/GLShaderProgram.hpp"
#include "Pine/Graphics/OpenGL/Texture/GLTexture.hpp"
#include "Pine/Graphics/OpenGL/VertexArray/GLVertexArray.hpp"
#include <GL/glew.h>
#include <stdexcept>

namespace
{

    std::uint32_t TranslateRenderMode(Pine::Graphics::RenderMode mode)
    {
        switch (mode)
        {
        case Pine::Graphics::RenderMode::Triangles:
            return GL_TRIANGLES;
        case Pine::Graphics::RenderMode::LineLoop:
            return GL_LINE_LOOP;
        default:
            throw std::runtime_error("Unsupported rendering mode.");
        }
    }

}

const char *Pine::Graphics::OpenGL::GetName() const
{
    return "OpenGL";
}

const char *Pine::Graphics::OpenGL::GetVersionString() const
{
    return m_VersionString.c_str();
}

const char *Pine::Graphics::OpenGL::GetGraphicsAdapter() const
{
    return m_GraphicsAdapter.c_str();
}

void Pine::Graphics::OpenGL::ClearBuffers(Pine::Graphics::Buffers buffers)
{
    int clearBits = 0;

    if (buffers & Buffers::ColorBuffer)
        clearBits |= GL_COLOR_BUFFER_BIT;
    if (buffers & Buffers::DepthBuffer)
        clearBits |= GL_DEPTH_BUFFER_BIT;
    if (buffers & Buffers::StencilBuffer)
        clearBits |= GL_STENCIL_BUFFER_BIT;

    if (clearBits == 0)
        return;

    glClear(clearBits);
}

void Pine::Graphics::OpenGL::ClearColor(Pine::Color color)
{
    glClearColor(static_cast<float>(color.r) / 255.f,
                 static_cast<float>(color.g) / 255.f,
                 static_cast<float>(color.b) / 255.f,
                 static_cast<float>(color.a) / 255.f);
}

bool Pine::Graphics::OpenGL::Setup()
{
    if (glewInit() != GLEW_OK)
        return false;

    m_VersionString = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    m_GraphicsAdapter = reinterpret_cast<const char*>(glGetString(GL_RENDERER));

    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &m_SupportedTextureSlots);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    return true;
}

void Pine::Graphics::OpenGL::Shutdown()
{
}

Pine::Graphics::IVertexArray* Pine::Graphics::OpenGL::CreateVertexArray()
{
    return new GLVertexArray();
}

void Pine::Graphics::OpenGL::DestroyVertexArray(Pine::Graphics::IVertexArray* array)
{
    array->Dispose();

    delete array;
}

Pine::Graphics::ITexture* Pine::Graphics::OpenGL::CreateTexture()
{
    return new GLTexture();
}

void Pine::Graphics::OpenGL::DestroyTexture(Pine::Graphics::ITexture* texture)
{
    texture->Dispose();

    delete dynamic_cast<GLTexture*>(texture);
}

Pine::Graphics::IShaderProgram* Pine::Graphics::OpenGL::CreateShaderProgram()
{
    return new GLShaderProgram();
}

void Pine::Graphics::OpenGL::DestroyShaderProgram(Pine::Graphics::IShaderProgram* program)
{
    program->Dispose();

    delete dynamic_cast<GLShaderProgram*>(program);
}

void Pine::Graphics::OpenGL::DrawArrays(Pine::Graphics::RenderMode mode, int count)
{
    glDrawArrays(TranslateRenderMode(mode), 0, count);
}

void Pine::Graphics::OpenGL::DrawElements(Pine::Graphics::RenderMode mode, int count)
{
    glDrawElements(TranslateRenderMode(mode), count, GL_UNSIGNED_INT, nullptr);
}

void Pine::Graphics::OpenGL::DrawArraysInstanced(Pine::Graphics::RenderMode mode, int count, int instanceCount)
{
    glDrawArraysInstanced(TranslateRenderMode(mode), 0, count, instanceCount);
}

void Pine::Graphics::OpenGL::DrawElementsInstanced(Pine::Graphics::RenderMode mode, int count, int instanceCount)
{
    glDrawElementsInstanced(TranslateRenderMode(mode), count, GL_UNSIGNED_INT, nullptr, instanceCount);
}

void Pine::Graphics::OpenGL::SetActiveTexture(int binding)
{
    glActiveTexture(GL_TEXTURE0 + binding);
}

int Pine::Graphics::OpenGL::GetSupportedTextureSlots()
{
    return m_SupportedTextureSlots;
}

Pine::Graphics::IFrameBuffer* Pine::Graphics::OpenGL::CreateFrameBuffer()
{
    return new GLFrameBuffer();
}

void Pine::Graphics::OpenGL::DestroyFrameBuffer(Pine::Graphics::IFrameBuffer* buffer)
{
    buffer->Dispose();

    delete dynamic_cast<GLFrameBuffer*>(buffer);
}

void Pine::Graphics::OpenGL::BindFrameBuffer(Pine::Graphics::IFrameBuffer* buffer)
{
    if (buffer)
        buffer->Bind();
    else
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Pine::Graphics::OpenGL::SetViewport(Pine::Vector2i position, Pine::Vector2i size)
{
    glViewport(position.x, position.y, size.x, size.y);
}

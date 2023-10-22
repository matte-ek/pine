#include "OpenGL.hpp"
#include "Pine/Graphics/OpenGL/FrameBuffer/GLFrameBuffer.hpp"
#include "Pine/Graphics/OpenGL/ShaderProgram/GLShaderProgram.hpp"
#include "Pine/Graphics/OpenGL/Texture/GLTexture.hpp"
#include "Pine/Graphics/OpenGL/VertexArray/GLVertexArray.hpp"
#include "Pine/Graphics/OpenGL/UniformBuffer/GLUniformBuffer.hpp"
#include "Pine/Core/Log/Log.hpp"
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

    std::uint32_t TranslateTestFunction(Pine::Graphics::TestFunction testFunction)
    {
        switch (testFunction)
        {
            case Pine::Graphics::TestFunction::Always:
                return GL_ALWAYS;
            case Pine::Graphics::TestFunction::Never:
                return GL_NEVER;
            case Pine::Graphics::TestFunction::Equal:
                return GL_EQUAL;
            case Pine::Graphics::TestFunction::NotEqual:
                return GL_NOTEQUAL;
            case Pine::Graphics::TestFunction::Greater:
                return GL_GREATER;
            case Pine::Graphics::TestFunction::GreaterEqual:
                return GL_GEQUAL;
            case Pine::Graphics::TestFunction::Less:
                return GL_LESS;
            case Pine::Graphics::TestFunction::LessEqual:
                return GL_LEQUAL;
            default:
                throw std::runtime_error("Invalid test function.");
        }
    }

    void GLAPIENTRY MessageCallback(GLenum source,
                                    GLenum type,
                                    GLuint id,
                                    GLenum severity,
                                    GLsizei length,
                                    const GLchar *message,
                                    const void *userParam)
    {

        // Ignoring this for now.
        if (severity == 0x826b)
        {
            return;
        }

        Pine::Log::Error(fmt::format("OpenGL 0x{:x}: {}", type, message));
    }

}

void Pine::Graphics::OpenGL::EnableErrorLogging()
{
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(MessageCallback, nullptr);
}

void Pine::Graphics::OpenGL::DisableErrorLogging()
{
    glDisable(GL_DEBUG_OUTPUT);
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

void Pine::Graphics::OpenGL::ClearBuffers(std::uint32_t buffers)
{
    int clearBits = 0;

    if (buffers & ColorBuffer)
        clearBits |= GL_COLOR_BUFFER_BIT;
    if (buffers & DepthBuffer)
        clearBits |= GL_DEPTH_BUFFER_BIT;
    if (buffers & StencilBuffer)
        clearBits |= GL_STENCIL_BUFFER_BIT;

    if (clearBits == 0)
        return;

    glClear(clearBits);
}

void Pine::Graphics::OpenGL::ClearColor(Color color)
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

    m_VersionString = reinterpret_cast<const char *>(glGetString(GL_VERSION));
    m_GraphicsAdapter = reinterpret_cast<const char *>(glGetString(GL_RENDERER));

    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &m_SupportedTextureSlots);

    // TODO: Figure out how this exactly works, using GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS doesn't work.
    m_SupportedTextureSlots = 16;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    return true;
}

void Pine::Graphics::OpenGL::Shutdown()
{
}

Pine::Graphics::IVertexArray *Pine::Graphics::OpenGL::CreateVertexArray()
{
    return new GLVertexArray();
}

void Pine::Graphics::OpenGL::DestroyVertexArray(IVertexArray*array)
{
    array->Dispose();

    delete array;
}

Pine::Graphics::ITexture *Pine::Graphics::OpenGL::CreateTexture()
{
    return new GLTexture();
}

void Pine::Graphics::OpenGL::DestroyTexture(ITexture*texture)
{
    texture->Dispose();

    delete dynamic_cast<GLTexture *>(texture);
}

Pine::Graphics::IShaderProgram *Pine::Graphics::OpenGL::CreateShaderProgram()
{
    return new GLShaderProgram();
}

void Pine::Graphics::OpenGL::DestroyShaderProgram(IShaderProgram*program)
{
    program->Dispose();

    delete dynamic_cast<GLShaderProgram *>(program);
}

void Pine::Graphics::OpenGL::DrawArrays(RenderMode mode, int count)
{
    glDrawArrays(TranslateRenderMode(mode), 0, count);
}

void Pine::Graphics::OpenGL::DrawElements(RenderMode mode, int count)
{
    glDrawElements(TranslateRenderMode(mode), count, GL_UNSIGNED_INT, nullptr);
}

void Pine::Graphics::OpenGL::DrawArraysInstanced(RenderMode mode, int count, int instanceCount)
{
    glDrawArraysInstanced(TranslateRenderMode(mode), 0, count, instanceCount);
}

void Pine::Graphics::OpenGL::DrawElementsInstanced(RenderMode mode, int count, int instanceCount)
{
    glDrawElementsInstanced(TranslateRenderMode(mode), count, GL_UNSIGNED_INT, nullptr, instanceCount);
}

int Pine::Graphics::OpenGL::GetSupportedTextureSlots()
{
    return m_SupportedTextureSlots;
}

Pine::Graphics::IFrameBuffer *Pine::Graphics::OpenGL::CreateFrameBuffer()
{
    return new GLFrameBuffer();
}

void Pine::Graphics::OpenGL::DestroyFrameBuffer(IFrameBuffer*buffer)
{
    buffer->Dispose();

    delete dynamic_cast<GLFrameBuffer *>(buffer);
}

void Pine::Graphics::OpenGL::BindFrameBuffer(IFrameBuffer*buffer)
{
    if (buffer)
        buffer->Bind();
    else
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Pine::Graphics::OpenGL::SetViewport(Vector2i position, Vector2i size)
{
    glViewport(position.x, position.y, size.x, size.y);
}

Pine::Graphics::IUniformBuffer *Pine::Graphics::OpenGL::CreateUniformBuffer()
{
    return new GLUniformBuffer();
}

void Pine::Graphics::OpenGL::DestroyUniformBuffer(IUniformBuffer*buffer)
{
    buffer->Dispose();

    delete dynamic_cast<GLUniformBuffer *>(buffer);
}

void Pine::Graphics::OpenGL::SetDepthTestEnabled(bool value)
{
    value ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
}

void Pine::Graphics::OpenGL::SetStencilTestEnabled(bool value)
{
    value ? glEnable(GL_STENCIL_TEST) : glDisable(GL_STENCIL_TEST);
}

void Pine::Graphics::OpenGL::SetFaceCullingEnabled(bool value)
{
    value ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);

    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glCullFace(GL_BACK);
}

void Pine::Graphics::OpenGL::SetDepthFunction(Pine::Graphics::TestFunction value)
{
    glDepthFunc(TranslateTestFunction(value));
}

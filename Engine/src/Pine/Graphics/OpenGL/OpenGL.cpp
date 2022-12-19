#include "OpenGL.hpp"
#include "Pine/Graphics/OpenGL/ShaderProgram/GLShaderProgram.hpp"
#include "Pine/Graphics/OpenGL/Texture/GLTexture.hpp"
#include "Pine/Graphics/OpenGL/VertexArray/GLVertexArray.hpp"
#include <GL/glew.h>

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

    delete texture;
}

Pine::Graphics::IShaderProgram* Pine::Graphics::OpenGL::CreateShaderProgram()
{
    return new GLShaderProgram();
}

void Pine::Graphics::OpenGL::DestroyShaderProgram(Pine::Graphics::IShaderProgram* program)
{
    program->Dispose();

    delete program;
}

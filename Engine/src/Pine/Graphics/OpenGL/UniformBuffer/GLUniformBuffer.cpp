#include "GLUniformBuffer.hpp"
#include <GL/glew.h>

void Pine::Graphics::GLUniformBuffer::Bind()
{
    glBindBuffer(GL_UNIFORM_BUFFER, m_Id);
}

void Pine::Graphics::GLUniformBuffer::Dispose()
{
    glDeleteBuffers(1, &m_Id);
}

void Pine::Graphics::GLUniformBuffer::Create(std::size_t size, int bindingIndex)
{
    glGenBuffers(1, &m_Id);

    glBindBuffer(GL_UNIFORM_BUFFER, m_Id);
    glBufferData(GL_UNIFORM_BUFFER, static_cast<std::int32_t>(size), nullptr, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, bindingIndex, m_Id);

    m_BindingIndex = bindingIndex;
}

void Pine::Graphics::GLUniformBuffer::UploadData(void* data, std::size_t size, std::size_t offset)
{
    glBufferSubData(GL_UNIFORM_BUFFER, static_cast<std::int32_t>(offset), static_cast<std::int32_t>(size), data);
}

int Pine::Graphics::GLUniformBuffer::GetBindIndex() const
{
    return m_BindingIndex;
}

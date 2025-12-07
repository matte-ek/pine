#include "GLUniformBuffer.hpp"
#include <GL/glew.h>
#include <limits>

namespace
{
    std::uint32_t m_BoundUniformBuffer = 0;
}

void Pine::Graphics::GLUniformBuffer::Bind()
{
    if (m_BoundUniformBuffer == m_Id)
    {
        return;
    }

    glBindBuffer(GL_UNIFORM_BUFFER, m_Id);

    m_BoundUniformBuffer = m_Id;
}

void Pine::Graphics::GLUniformBuffer::Dispose()
{
    glDeleteBuffers(1, &m_Id);
}

void Pine::Graphics::GLUniformBuffer::Create(std::size_t size, int bindingIndex)
{
    glGenBuffers(1, &m_Id);

    glBindBuffer(GL_UNIFORM_BUFFER, m_Id);
    glBufferData(GL_UNIFORM_BUFFER, static_cast<std::int32_t>(size), nullptr, GL_DYNAMIC_DRAW);
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

void Pine::Graphics::GLUniformBuffer::ResetChangeTracking()
{
    m_BoundUniformBuffer = std::numeric_limits<std::uint32_t>::max();
}

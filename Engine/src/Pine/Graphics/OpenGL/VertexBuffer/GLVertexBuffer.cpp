#include "GLVertexBuffer.hpp"
#include <GL/glew.h>

Pine::Graphics::GLVertexBuffer::GLVertexBuffer(const std::uint32_t id, const std::uint32_t binding, const std::size_t size)
    : m_Id(id),
      m_Binding(binding),
      m_Size(size)
{
}

void Pine::Graphics::GLVertexBuffer::Bind()
{
    glBindBuffer(GL_ARRAY_BUFFER, m_Id);
}

void Pine::Graphics::GLVertexBuffer::UploadData(const void* data, const std::size_t size, const std::size_t offset)
{
    glBufferSubData(GL_ARRAY_BUFFER, static_cast<std::int32_t>(offset), static_cast<std::int32_t>(size), data);
}

std::size_t Pine::Graphics::GLVertexBuffer::GetSize() const
{
    return m_Size;
}

bool Pine::Graphics::GLVertexBuffer::ReadData(void* destination, const std::size_t size, const std::size_t offset) const
{
    // Written this way around rather than as offset + size > m_Size, which would wrap on a size
    // near the top of its range and let the read through.
    const bool rangeFitsBuffer = offset <= m_Size && size <= m_Size - offset;

    if (!rangeFitsBuffer || (size != 0 && destination == nullptr))
    {
        return false;
    }

    // Read through GL_COPY_READ_BUFFER, the target that exists for exactly this: binding the
    // buffer to GL_ARRAY_BUFFER instead would knock out whatever the caller had bound there.
    GLint previousBuffer = 0;
    glGetIntegerv(GL_COPY_READ_BUFFER_BINDING, &previousBuffer);

    glBindBuffer(GL_COPY_READ_BUFFER, m_Id);
    glGetBufferSubData(GL_COPY_READ_BUFFER, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), destination);

    glBindBuffer(GL_COPY_READ_BUFFER, previousBuffer);

    return true;
}

void Pine::Graphics::GLVertexBuffer::SetDivisor(const VertexBufferDivisor mode, const int instanceCount)
{
    glVertexAttribDivisor(m_Binding, mode == VertexBufferDivisor::PerVertex ? 0 : instanceCount);
}

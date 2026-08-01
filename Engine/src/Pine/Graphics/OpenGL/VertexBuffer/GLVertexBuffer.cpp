#include "GLVertexBuffer.hpp"
#include <GL/glew.h>

Pine::Graphics::GLVertexBuffer::GLVertexBuffer(const std::uint32_t id, const std::uint32_t binding)
    : m_Id(id),
      m_Binding(binding)
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

void Pine::Graphics::GLVertexBuffer::SetDivisor(const VertexBufferDivisor mode, const int instanceCount)
{
    glVertexAttribDivisor(m_Binding, mode == VertexBufferDivisor::PerVertex ? 0 : instanceCount);
}

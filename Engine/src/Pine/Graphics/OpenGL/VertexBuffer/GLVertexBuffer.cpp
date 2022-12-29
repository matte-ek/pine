#include "GLVertexBuffer.hpp"
#include <GL/glew.h>

Pine::Graphics::GLVertexBuffer::GLVertexBuffer(std::uint32_t id, std::uint32_t binding)
    : m_Id(id),
      m_Binding(binding)
{
}

void Pine::Graphics::GLVertexBuffer::Bind()
{
    glBindBuffer(GL_ARRAY_BUFFER, m_Id);
}

void Pine::Graphics::GLVertexBuffer::UploadData(const void* data, size_t size, size_t offset)
{
    glBufferSubData(GL_ARRAY_BUFFER, static_cast<std::int32_t>(offset), static_cast<std::int32_t>(size), data);
}

void Pine::Graphics::GLVertexBuffer::SetDivisor(Pine::Graphics::VertexBufferDivisor mode, int instanceCount)
{
    glVertexAttribDivisor(m_Binding, mode == VertexBufferDivisor::PerVertex ? 0 : instanceCount);
}

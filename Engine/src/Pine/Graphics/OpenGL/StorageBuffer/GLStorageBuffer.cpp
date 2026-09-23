#include "GLStorageBuffer.hpp"
#include <GL/glew.h>
#include <stdexcept>

namespace
{

    // The same translation GLVertexArray makes for its buffers.
    GLenum TranslateBufferUsageHint(const Pine::Graphics::BufferUsageHint hint)
    {
        switch (hint)
        {
        case Pine::Graphics::BufferUsageHint::StaticDraw:
            return GL_STATIC_DRAW;
        case Pine::Graphics::BufferUsageHint::StreamDraw:
            return GL_STREAM_DRAW;
        case Pine::Graphics::BufferUsageHint::DynamicDraw:
            return GL_DYNAMIC_DRAW;
        default:
            throw std::runtime_error("Unsupported buffer usage hint.");
        }
    }

}

void Pine::Graphics::GLStorageBuffer::Create(const std::size_t size, const BufferUsageHint usageHint)
{
    glGenBuffers(1, &m_Id);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_Id);
    glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(size), nullptr, TranslateBufferUsageHint(usageHint));

    m_Size = size;
}

void Pine::Graphics::GLStorageBuffer::Dispose()
{
    glDeleteBuffers(1, &m_Id);

    m_Id = 0;
    m_Size = 0;
}

void Pine::Graphics::GLStorageBuffer::Bind(const int bindingIndex)
{
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingIndex, m_Id);
}

void Pine::Graphics::GLStorageBuffer::UploadData(const void* data, const std::size_t size, const std::size_t offset)
{
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_Id);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), data);
}

std::size_t Pine::Graphics::GLStorageBuffer::GetSize() const
{
    return m_Size;
}

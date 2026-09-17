#include "GLVertexArray.hpp"
#include "Pine/Graphics/OpenGL/VertexBuffer/GLVertexBuffer.hpp"

#include <GL/glew.h>
#include <stdexcept>

namespace
{

    std::uint32_t TranslateBufferUsageHint(const Pine::Graphics::BufferUsageHint hint)
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

Pine::Graphics::GLVertexArray::GLVertexArray()
{
    glGenVertexArrays(1, &m_Id);
}

void Pine::Graphics::GLVertexArray::Bind()
{
    glBindVertexArray(m_Id);
}

void Pine::Graphics::GLVertexArray::Dispose()
{
    glDeleteBuffers(static_cast<int>(m_BuffersIndices.size()), m_BuffersIndices.data());
    glDeleteVertexArrays(1, &m_Id);

    m_BuffersIndices.clear();
    m_ElementBuffer = 0;
    m_ElementBufferSize = 0;

    //for (auto buffer : m_Buffers)
    //    delete buffer;
}

Pine::Graphics::IVertexBuffer* Pine::Graphics::GLVertexArray::StoreFloatArrayBuffer(float *data, const std::size_t size, const int binding, const int vecSize, const BufferUsageHint hint)
{
    return StoreArrayBuffer(data, size, binding, vecSize, GL_FLOAT, hint);
}

Pine::Graphics::IVertexBuffer* Pine::Graphics::GLVertexArray::StoreIntArrayBuffer(float *data, const std::size_t size, const int binding, const int vecSize, const BufferUsageHint hint)
{
    return StoreArrayBuffer(data, size, binding, vecSize, GL_INT, hint);
}

void Pine::Graphics::GLVertexArray::StoreElementArrayBuffer(std::uint32_t *data, const std::size_t size)
{
    const auto buffer = CreateBuffer();

    // Bind and store the data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(size), reinterpret_cast<void*>(data), GL_STATIC_DRAW);

    // For element array buffers we don't have to do any binding stuff.

    m_ElementBuffer = buffer;
    m_ElementBufferSize = size;
}

bool Pine::Graphics::GLVertexArray::ReadElementArrayBuffer(void* destination, const std::size_t size, const std::size_t offset) const
{
    if (m_ElementBuffer == 0)
    {
        return false;
    }

    // As in GLVertexBuffer::ReadData: the subtraction cannot wrap, and the read goes through
    // GL_COPY_READ_BUFFER so that the caller's own bindings survive it. Binding the buffer to
    // GL_ELEMENT_ARRAY_BUFFER would be worse still, since that binding lives in whichever vertex
    // array happens to be bound.
    const bool rangeFitsBuffer = offset <= m_ElementBufferSize && size <= m_ElementBufferSize - offset;

    if (!rangeFitsBuffer || (size != 0 && destination == nullptr))
    {
        return false;
    }

    GLint previousBuffer = 0;
    glGetIntegerv(GL_COPY_READ_BUFFER_BINDING, &previousBuffer);

    glBindBuffer(GL_COPY_READ_BUFFER, m_ElementBuffer);
    glGetBufferSubData(GL_COPY_READ_BUFFER, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), destination);

    glBindBuffer(GL_COPY_READ_BUFFER, previousBuffer);

    return true;
}

std::uint32_t Pine::Graphics::GLVertexArray::CreateBuffer()
{
    std::uint32_t buffer;

    glGenBuffers(1, &buffer);

    m_BuffersIndices.push_back(buffer);

    return buffer;
}

std::uint32_t Pine::Graphics::GLVertexArray::GetId() const
{
    return m_Id;
}

Pine::Graphics::IVertexBuffer* Pine::Graphics::GLVertexArray::CreateFloatArrayBuffer(
        const std::size_t size, const int binding, const int vecSize, const BufferUsageHint usageHint)
{
    return CreateArrayBuffer(size, binding, vecSize, GL_FLOAT, usageHint);
}

Pine::Graphics::IVertexBuffer* Pine::Graphics::GLVertexArray::CreateIntegerArrayBuffer(
        const std::size_t size, const int binding, const int vecSize, const BufferUsageHint usageHint)
{
    return CreateArrayBuffer(size, binding, vecSize, GL_INT, usageHint);
}

Pine::Graphics::GLVertexBuffer* Pine::Graphics::GLVertexArray::CreateArrayBuffer(const std::size_t size, const int binding, const int vecSize, const int type,
    const BufferUsageHint hint)
{
    const auto buffer = CreateBuffer();

    // Bind and store the data
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, static_cast<long long>(size), nullptr, TranslateBufferUsageHint(hint));

    // Bind it to our VAO
    glVertexAttribPointer(binding, vecSize, type, false, 0, nullptr);

    // OpenGL remembers the current enabled buffers, so we'll just enable it once and leave it.
    glEnableVertexAttribArray(binding);

    auto vertexBuffer = new GLVertexBuffer(buffer, binding, size);

    m_Buffers.push_back(vertexBuffer);

    return vertexBuffer;
}

template <typename T>
Pine::Graphics::GLVertexBuffer* Pine::Graphics::GLVertexArray::StoreArrayBuffer(T *data, const std::size_t size, const int binding, const int vecSize, const int type, const BufferUsageHint hint)
{
    auto vertexBuffer = CreateArrayBuffer(size, binding, vecSize, type, hint);

    vertexBuffer->UploadData(data, size, 0);

    return vertexBuffer;
}

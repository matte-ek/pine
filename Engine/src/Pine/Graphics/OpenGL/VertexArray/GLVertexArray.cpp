#include "GLVertexArray.hpp"
#include "Pine/Graphics/OpenGL/VertexBuffer/GLVertexBuffer.hpp"

#include <GL/glew.h>
#include <stdexcept>

namespace
{

    std::uint32_t TranslateBufferUsageHint(Pine::Graphics::BufferUsageHint hint)
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

    //for (auto buffer : m_Buffers)
    //    delete buffer;
}

Pine::Graphics::IVertexBuffer* Pine::Graphics::GLVertexArray::StoreFloatArrayBuffer(float *data, std::size_t size, int binding, int vecSize, BufferUsageHint hint)
{
    return StoreArrayBuffer(data, size, binding, vecSize, GL_FLOAT, hint);
}

Pine::Graphics::IVertexBuffer* Pine::Graphics::GLVertexArray::StoreIntArrayBuffer(float *data, std::size_t size, int binding, int vecSize, BufferUsageHint hint)
{
    return StoreArrayBuffer(data, size, binding, vecSize, GL_INT, hint);
}

void Pine::Graphics::GLVertexArray::StoreElementArrayBuffer(std::uint32_t *data, std::size_t size)
{
    const auto buffer = CreateBuffer();

    // Bind and store the data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(size), reinterpret_cast<void*>(data), GL_STATIC_DRAW);

    // For element array buffers we don't have to do any binding stuff.
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
        std::size_t size, int binding, int vecSize, BufferUsageHint usageHint)
{
    return CreateArrayBuffer(size, binding, vecSize, GL_FLOAT, usageHint);
}

Pine::Graphics::IVertexBuffer* Pine::Graphics::GLVertexArray::CreateIntegerArrayBuffer(
        std::size_t size, int binding, int vecSize, BufferUsageHint usageHint)
{
    return CreateArrayBuffer(size, binding, vecSize, GL_INT, usageHint);
}

Pine::Graphics::GLVertexBuffer* Pine::Graphics::GLVertexArray::CreateArrayBuffer(std::size_t size, int binding, int vecSize, int type,
    BufferUsageHint hint)
{
    const auto buffer = CreateBuffer();

    // Bind and store the data
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, static_cast<long long>(size), nullptr, TranslateBufferUsageHint(hint));

    // Bind it to our VAO
    glVertexAttribPointer(binding, vecSize, type, false, 0, nullptr);

    // OpenGL remembers the current enabled buffers, so we'll just enable it once and leave it.
    glEnableVertexAttribArray(binding);

    auto vertexBuffer = new GLVertexBuffer(buffer, binding);

    m_Buffers.push_back(vertexBuffer);

    return vertexBuffer;
}

template <typename T>
Pine::Graphics::GLVertexBuffer* Pine::Graphics::GLVertexArray::StoreArrayBuffer(T *data, std::size_t size, int binding, int vecSize, int type, BufferUsageHint hint)
{
    auto vertexBuffer = CreateArrayBuffer(size, binding, vecSize, type, hint);

    vertexBuffer->UploadData(data, size, 0);

    return vertexBuffer;
}

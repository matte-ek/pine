#include "GLVertexArray.hpp"

#include <GL/glew.h>

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
    glDeleteBuffers(static_cast<int>(m_Buffers.size()), m_Buffers.data());
    glDeleteVertexArrays(1, &m_Id);

    m_Buffers.clear();
}

void Pine::Graphics::GLVertexArray::StoreFloatArrayBuffer(const std::vector<float>& vec, int binding, int vecSize)
{
    StoreArrayBuffer(vec, binding, vecSize, GL_FLOAT);
}

void Pine::Graphics::GLVertexArray::StoreIntArrayBuffer(const std::vector<int>& vec, int binding, int vecSize)
{
    StoreArrayBuffer(vec, binding, vecSize, GL_INT);
}

void Pine::Graphics::GLVertexArray::StoreElementArrayBuffer(const std::vector<int>& vec)
{
    const auto buffer = CreateBuffer();

    // Bind and store the data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<long long>(vec.size() * sizeof(int)), vec.data(), GL_STATIC_DRAW);

    // For element array buffers we don't have to do any binding stuff.
}

std::uint32_t Pine::Graphics::GLVertexArray::CreateBuffer()
{
    std::uint32_t buffer;

    glGenBuffers(1, &buffer);

    m_Buffers.push_back(buffer);

    return buffer;
}

std::uint32_t Pine::Graphics::GLVertexArray::GetId() const
{
    return m_Id;
}

template <typename T>
void Pine::Graphics::GLVertexArray::StoreArrayBuffer(const std::vector<T>& vec, int binding, int vecSize, int type)
{
    const auto buffer = CreateBuffer();

    // Bind and store the data
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, static_cast<long long>(vec.size() * sizeof(T)), vec.data(), GL_STATIC_DRAW);

    // Bind it to our VAO
    glVertexAttribPointer(binding, vecSize, type, false, 0, nullptr);

    // OpenGL remembers the current enabled buffers, so we'll just enable it once and leave it.
    glEnableVertexAttribArray(binding);
}

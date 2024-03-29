#pragma once
#include "IVertexBuffer.hpp"
#include <cstddef>

namespace Pine::Graphics
{

    enum class BufferUsageHint
    {
        StaticDraw,
        DynamicDraw,
        StreamDraw
    };

    class IVertexArray
    {
    private:
    public:
        IVertexArray() = default;
        virtual ~IVertexArray() = default;

        virtual void Bind() = 0;
        virtual void Dispose() = 0;

        // Pre-allocates a vertex buffer with a specified size
        virtual IVertexBuffer* CreateFloatArrayBuffer(std::size_t size, int binding, int vecSize, BufferUsageHint usageHint) = 0;
        virtual IVertexBuffer* CreateIntegerArrayBuffer(std::size_t size, int binding, int vecSize, BufferUsageHint usageHint) = 0;

        // Stores an array in a vertex buffer object, then binds the object to the vertex array at the
        // specified binding index. vecSize is a hint that specifies the array dimension.
        virtual IVertexBuffer* StoreFloatArrayBuffer(float *data, std::size_t size, int binding, int vecSize, BufferUsageHint usageHint) = 0;
        virtual IVertexBuffer* StoreIntArrayBuffer(float *data, std::size_t size, int binding, int vecSize, BufferUsageHint usageHint) = 0;

        // The element array buffer (of index buffer) is an array that specifies the order
        // of how vertices should be rendered, allowing you to save on vertex data.
        virtual void StoreElementArrayBuffer(std::uint32_t *data, std::size_t size) = 0;
    };

}


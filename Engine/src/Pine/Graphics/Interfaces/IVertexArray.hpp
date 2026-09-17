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

        // Copies size bytes of that element array buffer back into destination, under the same
        // terms as IVertexBuffer::ReadData: synchronous, it stalls until the data arrives, and it
        // leaves the caller's bindings alone. An array that was never given an element buffer, or
        // a range reaching past the end of the one it has, is refused with false.
        virtual bool ReadElementArrayBuffer(void* destination, std::size_t size, std::size_t offset = 0) const = 0;
    };

}

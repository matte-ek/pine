#pragma once
#include <cstddef>

namespace Pine::Graphics
{

    enum class VertexBufferDivisor
    {
        PerVertex,
        PerInstance,
    };

    // Creating/Disposing of this vertex buffer should be taken care of by IVertexArray
    class IVertexBuffer
    {
    private:
    public:
        virtual ~IVertexBuffer() = default;

        virtual void Bind() = 0;
        virtual void UploadData(const void* data, std::size_t size, std::size_t offset) = 0;
        virtual void SetDivisor(VertexBufferDivisor mode, int instanceCount) = 0;
    };

}
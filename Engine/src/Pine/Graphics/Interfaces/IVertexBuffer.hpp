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

        // Copies size bytes of the buffer back into destination, which the caller has to have
        // that much room in. Synchronous: it needs the graphics context and stalls until the data
        // arrives, so it belongs in tooling rather than in a frame. A range reaching past the end
        // of the buffer is refused with false and nothing written. Whatever the caller had bound
        // is left bound.
        virtual bool ReadData(void* destination, std::size_t size, std::size_t offset = 0) const = 0;

        // How many bytes the buffer was allocated with. Worth checking before an UploadData that
        // does not start from the beginning, since writing past the end of a GPU buffer is not an
        // error the graphics API reports back.
        virtual std::size_t GetSize() const = 0;

        virtual void SetDivisor(VertexBufferDivisor mode, int instanceCount) = 0;
    };

}

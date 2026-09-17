#pragma once
#include <cstdint>
#include <vector>

#include "Pine/Graphics/Interfaces/IVertexArray.hpp"

namespace Pine::Graphics
{
    class GLVertexBuffer;

    // OpenGL implementation of a vertex array
    class GLVertexArray : public IVertexArray
    {
    private:
        std::uint32_t m_Id = 0;

        // The element array buffer StoreElementArrayBuffer last created, and how many bytes it
        // holds. Remembered so that ReadElementArrayBuffer can reach it without having to bind
        // this vertex array first.
        std::uint32_t m_ElementBuffer = 0;
        std::size_t m_ElementBufferSize = 0;

        std::vector<IVertexBuffer*> m_Buffers;
        std::vector<std::uint32_t> m_BuffersIndices;

        std::uint32_t CreateBuffer();

        GLVertexBuffer* CreateArrayBuffer(std::size_t size, int binding, int vecSize, int type, BufferUsageHint hint);

        template <typename T>
        GLVertexBuffer* StoreArrayBuffer(T *data, std::size_t size, int binding, int vecSize, int type, BufferUsageHint hint);
    public:
        GLVertexArray();

        void Bind() override;
        void Dispose() override;

        IVertexBuffer* CreateFloatArrayBuffer(std::size_t size, int binding, int vecSize, BufferUsageHint usageHint) override;
        IVertexBuffer* CreateIntegerArrayBuffer(std::size_t size, int binding, int vecSize, BufferUsageHint usageHint) override;

        IVertexBuffer* StoreFloatArrayBuffer(float *data, std::size_t size, int binding, int vecSize, BufferUsageHint hint) override;
        IVertexBuffer* StoreIntArrayBuffer(float *data, std::size_t size, int binding, int vecSize, BufferUsageHint hint) override;
        void StoreElementArrayBuffer(std::uint32_t *data, std::size_t size) override;
        bool ReadElementArrayBuffer(void* destination, std::size_t size, std::size_t offset = 0) const override;

        std::uint32_t GetId() const;
    };

}

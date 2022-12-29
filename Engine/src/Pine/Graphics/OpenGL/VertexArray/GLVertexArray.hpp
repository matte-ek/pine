#pragma once
#include <cstdint>

#include "Pine/Graphics/Interfaces/IVertexArray.hpp"

namespace Pine::Graphics
{
    class GLVertexBuffer;

    // OpenGL implementation of a vertex array
    class GLVertexArray : public IVertexArray
    {
    private:
        std::uint32_t m_Id = 0;

        std::vector<IVertexBuffer*> m_Buffers;
        std::vector<std::uint32_t> m_BuffersIndices;

        std::uint32_t CreateBuffer();

        GLVertexBuffer* CreateArrayBuffer(size_t size, int binding, int vecSize, int type, BufferUsageHint hint);

        template <typename T>
        GLVertexBuffer* StoreArrayBuffer(const std::vector<T>& vec, int binding, int vecSize, int type, BufferUsageHint hint);
    public:
        GLVertexArray();

        void Bind() override;
        void Dispose() override;

        IVertexBuffer* CreateFloatArrayBuffer(size_t size, int binding, int vecSize, BufferUsageHint usageHint) override;
        IVertexBuffer* CreateIntegerArrayBuffer(size_t size, int binding, int vecSize, BufferUsageHint usageHint) override;

        IVertexBuffer* StoreFloatArrayBuffer(const std::vector<float>& vec, int binding, int vecSize, BufferUsageHint hint) override;
        IVertexBuffer* StoreIntArrayBuffer(const std::vector<int>& vec, int binding, int vecSize, BufferUsageHint hint) override;
        void StoreElementArrayBuffer(const std::vector<int>& vec) override;

        std::uint32_t GetId() const;
    };

}
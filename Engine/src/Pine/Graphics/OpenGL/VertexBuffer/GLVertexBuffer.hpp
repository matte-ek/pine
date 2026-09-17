#pragma once
#include <cstdint>

#include "Pine/Graphics/Interfaces/IVertexBuffer.hpp"

namespace Pine::Graphics
{

    class GLVertexBuffer : public IVertexBuffer
    {
    private:
        std::uint32_t m_Id = 0;
        std::uint32_t m_Binding = 0;
        std::size_t m_Size = 0;
    public:
        GLVertexBuffer(std::uint32_t id, std::uint32_t bindingIndex, std::size_t size);

        void Bind() override;
        void UploadData(const void* data, std::size_t size, std::size_t offset) override;

        std::size_t GetSize() const override;

        void SetDivisor(VertexBufferDivisor mode, int instanceCount) override;
    };

}
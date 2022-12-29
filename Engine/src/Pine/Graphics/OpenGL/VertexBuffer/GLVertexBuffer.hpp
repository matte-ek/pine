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
    public:
        explicit GLVertexBuffer(std::uint32_t id, std::uint32_t bindingIndex);

        void Bind() override;
        void UploadData(const void* data, size_t size, size_t offset) override;

        void SetDivisor(VertexBufferDivisor mode, int instanceCount) override;
    };

}
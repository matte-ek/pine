#pragma once
#include <cstdint>

#include "Pine/Graphics/Interfaces/IUniformBuffer.hpp"

namespace Pine::Graphics
{

    class GLUniformBuffer : public IUniformBuffer
    {
    private:
        std::uint32_t m_Id = 0;
    public:

        void Bind() override;
        void Dispose() override;

        void Create(size_t size, int bindingIndex) override;
        void UploadData(void* data, size_t size, size_t offset) override;

    };

}
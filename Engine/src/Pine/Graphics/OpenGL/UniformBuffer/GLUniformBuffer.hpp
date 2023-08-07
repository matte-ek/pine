#pragma once
#include <cstdint>

#include "Pine/Graphics/Interfaces/IUniformBuffer.hpp"

namespace Pine::Graphics
{

    class GLUniformBuffer : public IUniformBuffer
    {
    private:
        std::uint32_t m_Id = 0;
        int m_BindingIndex = 0;
    public:

        void Bind() override;
        void Dispose() override;

        int GetBindIndex() const override;

        void Create(std::size_t size, int bindingIndex) override;
        void UploadData(void* data, std::size_t size, std::size_t offset) override;

    };

}
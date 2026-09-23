#pragma once
#include <cstdint>

#include "Pine/Graphics/Interfaces/IStorageBuffer.hpp"

namespace Pine::Graphics
{

    class GLStorageBuffer : public IStorageBuffer
    {
    private:
        std::uint32_t m_Id = 0;
        std::size_t m_Size = 0;
    public:
        void Create(std::size_t size, BufferUsageHint usageHint) override;
        void Dispose() override;

        void Bind(int bindingIndex) override;

        void UploadData(const void* data, std::size_t size, std::size_t offset) override;

        std::size_t GetSize() const override;
    };

}

#pragma once

#include <cstddef>

namespace Pine::Graphics
{

    class IUniformBuffer
    {
    public:
        virtual ~IUniformBuffer() = default;

        virtual void Bind() = 0;
        virtual void Dispose() = 0;

        virtual void Create(size_t size, int bindingIndex) = 0;
        virtual void UploadData(void* data, size_t size, size_t offset) = 0;
    };

}
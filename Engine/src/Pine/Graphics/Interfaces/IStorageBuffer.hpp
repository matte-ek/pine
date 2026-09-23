#pragma once

#include "IVertexArray.hpp"

#include <cstddef>

namespace Pine::Graphics
{

    // A block of GPU memory a shader reads as an array of structs, sized by the buffer rather than
    // by the shader - unlike a uniform buffer, whose arrays have a fixed length and a small size cap.
    // For data too large or too variable in length for a uniform buffer, such as the placements of
    // thousands of instances.
    class IStorageBuffer
    {
    public:
        virtual ~IStorageBuffer() = default;

        // Allocates 'size' bytes, leaving their contents undefined until UploadData writes them.
        virtual void Create(std::size_t size, BufferUsageHint usageHint) = 0;
        virtual void Dispose() = 0;

        // Makes the buffer what the shader's storage block at 'bindingIndex' reads.
        virtual void Bind(int bindingIndex) = 0;

        // Has to fit inside the size the buffer was created with, which GetSize reports.
        virtual void UploadData(const void* data, std::size_t size, std::size_t offset) = 0;

        virtual std::size_t GetSize() const = 0;
    };

}

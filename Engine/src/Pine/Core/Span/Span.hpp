#pragma once

namespace Pine
{
    class ByteSpan
    {
    public:
        std::byte* data = nullptr;
        size_t size = 0;

        ByteSpan() = default;

        ByteSpan(const std::byte* data, size_t size)
        {
            this->data = new std::byte[size];
            this->size = size;

            memcpy(this->data, data, size);
        }

        ByteSpan(const ByteSpan& other)
        {
            data = new std::byte[other.size];
            size = other.size;

            memcpy(this->data, other.data, other.size);
        }

        ByteSpan(ByteSpan&& other) noexcept
        {
            data = other.data;
            size = other.size;

            other.data = nullptr;
            other.size = 0;
        }

        ~ByteSpan()
        {
            delete[] data;
        }

        ByteSpan& operator=(const ByteSpan& other)
        {
            if (this != &other)
            {
                data = new std::byte[other.size];
                size = other.size;

                memcpy(this->data, other.data, other.size);
            }

            return *this;
        }

        ByteSpan& operator=(ByteSpan&& other) noexcept
        {
            if (this != &other)
            {
                data = other.data;
                size = other.size;

                other.data = nullptr;
                other.size = 0;
            }

            return *this;
        }
    };
}

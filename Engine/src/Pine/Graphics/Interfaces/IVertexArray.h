#pragma once
#include <vector>

namespace Pine::Graphics
{

    using Buffer = void*;

    class IVertexArray
    {
    private:
    public:
        IVertexArray() = default;
        virtual ~IVertexArray() = default;

        virtual void Bind() = 0;
        virtual void Dispose() = 0;

        // Stores an array in a buffer object, then binds the object to the vertex array at the
        // specified binding index. vecSize is a hint that specifies the array dimension.
        virtual void StoreFloatArrayBuffer(const std::vector<float>& vec, int binding, int vecSize) = 0;
        virtual void StoreIntArrayBuffer(const std::vector<int>& vec, int binding, int vecSize) = 0;

        virtual void StoreElementArrayBuffer(const std::vector<int>& vec) = 0;
    };

}


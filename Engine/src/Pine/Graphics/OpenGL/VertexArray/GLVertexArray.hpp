#pragma once
#include <cstdint>

#include "Pine/Graphics/Interfaces/IVertexArray.h"

namespace Pine::Graphics
{

    class GLVertexArray : public IVertexArray
    {
    private:
        std::uint32_t m_Id = 0;

        std::vector<std::uint32_t> m_Buffers;

        std::uint32_t CreateBuffer();

        template <typename T>
        void StoreArrayBuffer(const std::vector<T>& vec, int binding, int vecSize, int type);
    public:
        GLVertexArray();

        void Bind() override;
        void Dispose() override;

        void StoreFloatArrayBuffer(const std::vector<float>& vec, int binding, int vecSize) override;
        void StoreIntArrayBuffer(const std::vector<int>& vec, int binding, int vecSize) override;

        void StoreElementArrayBuffer(const std::vector<int>& vec) override;

        std::uint32_t GetId() const;
    };

}
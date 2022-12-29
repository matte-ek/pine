#pragma once

#include "Pine/Core/Math/Math.hpp"

namespace Pine::Graphics
{

    enum Buffers
    {
        ColorBuffer = (1 << 0),
        DepthBuffer = (1 << 1),
        StencilBuffer = (1 << 2),
        NormalBuffer = (1 << 3)
    };

    class ITexture;

    class IFrameBuffer
    {
    private:
    public:
        virtual ~IFrameBuffer() = default;

        virtual ITexture* GetColorBuffer() = 0;
        virtual ITexture* GetDepthBuffer() = 0;
        virtual ITexture* GetNormalBuffer() = 0;

        virtual Vector2i GetSize() = 0;

        virtual void Bind() = 0;
        virtual void Dispose() = 0;

        virtual bool Create(int width, int height, Buffers buffers) = 0;
    };

}
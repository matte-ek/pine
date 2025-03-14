#pragma once

#include "ITexture.hpp"

#include "Pine/Core/Math/Math.hpp"

namespace Pine::Graphics
{

    enum Buffers
    {
        ColorBuffer = (1 << 0),
        DepthBuffer = (1 << 1),
        StencilBuffer = (1 << 2), // Notice: When creating a stencil buffer, a depth buffer must also be created, due to internal implementations.
        NormalBuffer = (1 << 3)
    };

    enum class ReadFormat
    {
        RGBA,
        RGB,
        RG,
        R,
        Depth,
        Stencil
    };

    class IFrameBuffer
    {
    private:
    public:
        virtual ~IFrameBuffer() = default;

        virtual ITexture* GetColorBuffer() = 0;
        virtual ITexture* GetDepthBuffer() = 0;
        virtual ITexture* GetDepthStencilBuffer() = 0;
        virtual ITexture* GetNormalBuffer() = 0;

        virtual Vector2i GetSize() = 0;

        virtual void Bind() = 0;
        virtual void Dispose() = 0;

        virtual void Blit(IFrameBuffer* source, Buffers buffer = ColorBuffer, Vector4i srcRect = Vector4i(-1), Vector4i dstRect = Vector4i(-1)) = 0;

        virtual void ReadPixels(Vector2i position, Vector2i size, ReadFormat readFormat, TextureDataFormat dataFormat, size_t bufferSize, void* buffer) = 0;

        virtual bool Create(int width, int height, std::uint32_t buffers, int multiSample = 0) = 0;
    };

}
#pragma once

#include "ITexture.hpp"
#include "IVertexArray.hpp"
#include "Pine/Core/Color/Color.hpp"

namespace Pine::Graphics
{

    enum Buffers
    {
        ColorBuffer = (1 << 0),
        DepthBuffer = (1 << 1),
        StencilBuffer = (1 << 2)
    };

    class IGraphicsAPI
    {
    public:
        IGraphicsAPI() = default;
        virtual ~IGraphicsAPI() = default;

        virtual bool Setup() = 0;
        virtual void Shutdown() = 0;

        // Example: OpenGL
        virtual const char* GetName() const = 0;

        // Graphics API version only
        virtual const char* GetVersionString() const = 0;

        // The name of the GPU
        virtual const char* GetGraphicsAdapter() const = 0;

        virtual void ClearBuffers(Buffers buffers) = 0;
        virtual void ClearColor(Color color) = 0;

        virtual IVertexArray* CreateVertexArray() = 0;
        virtual void DestroyVertexArray(IVertexArray* array) = 0;

        virtual ITexture* CreateTexture() = 0;
        virtual void DestroyTexture(ITexture* texture) = 0;

    };

}
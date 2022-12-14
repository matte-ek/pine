#pragma once

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
        virtual const char* GetName() = 0;

        // Graphics API version only
        virtual const char* GetVersionString() = 0;

        // The name of the GPU
        virtual const char* GetGraphicsAdapter() = 0;

        virtual void ClearBuffers(Buffers buffers) = 0;
        virtual void ClearColor(Color color) = 0;
    };

}
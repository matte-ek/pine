#pragma once
#include "Pine/Graphics/Interfaces/IFrameBuffer.hpp"

namespace Pine::Rendering::Common::Blur
{
    struct BlurContext
    {
        Graphics::IFrameBuffer* IntermediateBuffer = nullptr;
        Graphics::IFrameBuffer* TargetBuffer = nullptr;

        int Width = 0;
        int Height = 0;

        bool UseSingleChannel = false;

        // When set, the ping-pong buffers are RGBA16F instead of 8-bit, so values above 1.0 survive
        // the blur (needed for HDR bloom; without it bright pixels clamp to white before blurring).
        bool UseHDR = false;

        int PassCount = 3;

        void Create();
        void Destroy();
    };

    void Setup();
    void Shutdown();

    void Run(const BlurContext& context);
}

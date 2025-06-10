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

        int PassCount = 3;

        void Create();
        void Destroy();
    };

    void Setup();
    void Shutdown();

    void Run(const BlurContext& context);
}

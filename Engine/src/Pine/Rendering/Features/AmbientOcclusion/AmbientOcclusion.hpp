#pragma once
#include "Pine/Rendering/RenderingContext.hpp"

namespace Pine::Rendering::AmbientOcclusion
{
    Graphics::ITexture* GetOutputTexture();

    void UseDepthBuffer(Graphics::IFrameBuffer* buffer);

    void Run(const RenderingContext& context);

    void Setup();
    void Shutdown();
}

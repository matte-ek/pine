#pragma once

#include "Pine/Graphics/Interfaces/IFrameBuffer.hpp"

namespace RenderHandler
{
    void Setup();
    void Shutdown();

    Pine::Graphics::IFrameBuffer* GetRenderFrameBuffer();
}

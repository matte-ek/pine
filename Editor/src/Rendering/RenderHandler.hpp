#pragma once

#include "Pine/Graphics/Interfaces/IFrameBuffer.hpp"

namespace RenderHandler
{
    void Setup();
    void Shutdown();

    Pine::Graphics::IFrameBuffer* GetGameFrameBuffer();
    Pine::Graphics::IFrameBuffer* GetLevelFrameBuffer();
}
#pragma once

#include "Pine/Graphics/Interfaces/IFrameBuffer.hpp"
#include "Pine/Rendering/RenderingContext.hpp"

namespace Editor::RenderHandler
{
    void Setup();
    void Shutdown();

    Pine::RenderingContext* GetLevelRenderingContext();
    Pine::RenderingContext* GetGameRenderingContext();

    Pine::Graphics::IFrameBuffer* GetGameFrameBuffer();
    Pine::Graphics::IFrameBuffer* GetLevelFrameBuffer();
}
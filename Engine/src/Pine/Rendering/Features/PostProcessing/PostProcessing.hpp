#pragma once
#include <Pine/Rendering/RenderManager/RenderManager.hpp>

namespace Pine::Rendering::PostProcessing
{
    void Setup();
    void Shutdown();

    void Render(const RenderingContext* renderingContext, Graphics::IFrameBuffer* sceneFrameBuffer);
}

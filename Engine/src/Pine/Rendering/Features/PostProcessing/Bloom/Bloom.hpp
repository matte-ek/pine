#pragma once
#include "Pine/Rendering/RenderingContext.hpp"

namespace Pine::Graphics
{
    class IFrameBuffer;
}

namespace Pine::Rendering::Bloom
{
    // The blurred bright-pass result, composited additively into the scene in the post-process pass.
    Graphics::ITexture* GetOutputTexture();

    // Extract bright areas from the (HDR) scene buffer, blur them, and store the glow in the output.
    // Must run after the scene has been rendered into sceneFrameBuffer, before the post-process resolve.
    void Run(const RenderingContext& context, Graphics::IFrameBuffer* sceneFrameBuffer);

    // Blacks out the glow buffer so the post-process composite adds nothing. Called when bloom is
    // disabled, so the last glow isn't left frozen on screen; cheap to call repeatedly.
    void ClearOutput();

    void Setup();
    void Shutdown();
}

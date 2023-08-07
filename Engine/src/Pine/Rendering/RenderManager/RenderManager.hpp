#pragma once
#include "Pine/Rendering/RenderingContext.hpp"
#include <functional>

namespace Pine
{
    enum class RenderStage
    {
        PreRender,
        Render2D,
        Render3D,
        PostProcessing,
        PostRender
    };
}

namespace Pine::RenderManager
{
    // Called internally by the engine
    void Setup();
    void Shutdown();
    void Run();

    // Allows the user to provide a function pointer which will be called during specific
    // render events shown in RenderStage
    void AddRenderCallback(const std::function<void(RenderStage, float)>& func);

    void SetPrimaryRenderingContext(RenderingContext* context);
    RenderingContext* GetPrimaryRenderingContext();

    // Pine supports multiple rendering contexts at the same time, and will redo the entire rendering on the other context.
    // This could be used for example with an editor and game camera, rendered at the same time.
    void AddRenderingContextPass(RenderingContext* context);
    void RemoveRenderingContextPass(RenderingContext* context);

    // The rendering context currently being used during rendering.
    RenderingContext* GetCurrentRenderingContext();

    // The default rendering context's properties may be overwritten, but is generally used
    // as a 'reset' for the rendering pipeline.
    RenderingContext* GetDefaultRenderingContext();

}
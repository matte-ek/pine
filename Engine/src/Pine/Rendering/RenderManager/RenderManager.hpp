#pragma once
#include "Pine/Rendering/RenderingContext.hpp"
#include <functional>

namespace Pine
{
    enum class RenderStage
    {
        PreRender,
        PostRender,
        RenderContext,
        PreRender2D,
        PostRender2D,
        PreRender3D,
        PostRender3D,
        PostProcessing
    };

    enum class PipelineStage
    {
        Prepass,
        Default
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
    void AddRenderCallback(const std::function<void(RenderingContext*, RenderStage, float)>& func);

    void SetPrimaryRenderingContext(RenderingContext* context);
    RenderingContext* GetPrimaryRenderingContext();

    // Pine supports multiple rendering contexts at the same time, and will redo the entire rendering on the other context.
    // This could be used for example with an editor and game camera, rendered at the same time.
    void AddRenderingContextPass(RenderingContext* context);
    void RemoveRenderingContextPass(const RenderingContext* context);

    // The rendering context currently being used during rendering.
    RenderingContext* GetCurrentRenderingContext();

    // Every context that will be rendered this frame, primary first.
    //
    // Needed by scene-level work that happens once per frame but has to answer a question about
    // "the viewer" - which viewer is genuinely ambiguous with an editor viewport and a game camera
    // both live, and the primary context is the *game* one in the editor, not the one being looked
    // at. Answering against all of them is the only reading that is right in both.
    const std::vector<RenderingContext*>& GetRenderingContexts();

    // The default rendering context's properties may be overwritten, but is generally used
    // as a 'reset' for the rendering pipeline.
    RenderingContext* GetDefaultRenderingContext();

    // TODO: I don't like having to expose this.
    Graphics::IFrameBuffer* GetInternalFrameBuffer();

    double GetGlobalDeltaTime();

}
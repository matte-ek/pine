#pragma once
#include <functional>

namespace Pine
{
    enum class RenderStage
    {
        PreRender,
        Render2D,
        Render3D,
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
    void AddRenderCallback(const std::function<void(RenderStage)>& func);
}
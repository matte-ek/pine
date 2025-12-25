#pragma once

#include "Pine/Assets/Material/Material.hpp"
#include "Pine/Rendering/RenderingContext.hpp"

namespace Pine
{
    enum class PipelineStage;
    class Model;
    class ModelRenderer;
    class Light;
}

namespace Pine::Pipeline3D
{
    struct PipelineConfiguration
    {
        bool RenderShadows = true;
        bool RenderSkybox = true;
    };

    void Setup();
    void Shutdown();

    void Prepare();
    void Run(RenderingContext& context, PipelineStage stage);

    PipelineConfiguration& GetPipelineConfiguration();

    Graphics::ITexture* GetPositionTexture();
}

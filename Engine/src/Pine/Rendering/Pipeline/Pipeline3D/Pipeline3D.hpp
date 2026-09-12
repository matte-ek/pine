#pragma once

#include "Pine/Assets/Material/Material.hpp"
#include "Pine/Rendering/RenderingContext.hpp"
#include "Pine/Rendering/SceneProcessor/SceneProcessor.hpp"

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
        bool RenderAmbientOcclusion = true;
        bool RenderBloom = true;
    };

    void Setup();
    void Shutdown();

    void Prepare();
    void Run(RenderingContext& context, PipelineStage stage);

    // Draws one object batch, filtered to a material rendering mode and to a visibility set.
    //
    // Exposed because the shadow pass is a second caller: it renders the same batch, from a
    // different projection, with a shader override and its own visibility. It previously kept a
    // near-copy of this function, which is the thing worth deleting rather than extending.
    void RenderBatch(const Rendering::ObjectBatchMap& mapBatch,
                     MaterialRenderingMode materialRenderingMode,
                     const Rendering::RenderCulling::VisibilitySet& visibility);

    PipelineConfiguration& GetPipelineConfiguration();

    Graphics::ITexture* GetPositionTexture();
}

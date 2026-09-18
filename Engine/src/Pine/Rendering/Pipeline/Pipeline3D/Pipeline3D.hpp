#pragma once

#include "Pine/Assets/Material/Material.hpp"
#include "Pine/Rendering/DrawList/DrawList.hpp"
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

    // Draws an already built and ordered draw list, one instanced draw per run of consecutive
    // items sharing a mesh and a material.
    //
    // Takes the list rather than the batch because ordering belongs to the view and this does not:
    // the caller decides what its pass wants (see Rendering::DrawOrder) and this submits whatever
    // order it is handed.
    //
    // Exposed because the shadow pass is a second caller: it draws the same scene from a different
    // projection, with a shader override and its own visibility. It previously kept a near-copy of
    // this function, which is the thing worth deleting rather than extending.
    void RenderBatch(const Rendering::DrawList& drawList);

    PipelineConfiguration& GetPipelineConfiguration();

    Graphics::ITexture* GetPositionTexture();
}

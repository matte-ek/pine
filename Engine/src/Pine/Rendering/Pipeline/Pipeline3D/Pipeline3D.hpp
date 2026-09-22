#pragma once

#include "Pine/Assets/Material/Material.hpp"
#include "Pine/Graphics/Interfaces/IGraphicsAPI.hpp"
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

    // How the rasterizer treats a surface: which of its faces survive, and how far the depth it
    // writes is pushed. Together because geometry drawn without culling is what needs the bias.
    struct RasterState
    {
        // False switches culling off entirely, and leaves FaceCulling meaning nothing.
        bool CullFaces = true;

        Graphics::FaceCullMode FaceCulling = Graphics::FaceCullMode::Back;

        // Ignored wherever depth bias is switched off, which is every pass but the shadow one.
        float SlopeBias = 0.f;
        float DepthBias = 0.f;
    };

    // The two states RenderBatch draws a pass's list with: one for the pass's own geometry, and one
    // for runs whose material is MaterialRenderFace::Both.
    struct BatchRasterState
    {
        // Applied by RenderBatch before its first draw.
        RasterState Default;

        // Culling off. ShadowPass also sets a bias here, since open geometry cannot take its
        // separation from front-face culling.
        RasterState TwoSided = { false, Graphics::FaceCullMode::Back, 0.f, 0.f };
    };

    void Setup();
    void Shutdown();

    void Prepare();
    void Run(RenderingContext& context, PipelineStage stage);

    // Draws an already built and ordered draw list, one instanced draw per run of consecutive
    // items sharing a mesh and a material. The caller chooses the order (see Rendering::DrawOrder).
    //
    // Applies rasterState.Default before drawing, switches to rasterState.TwoSided for two-sided
    // runs, and leaves the rasterizer in rasterState.Default.
    void RenderBatch(const Rendering::DrawList& drawList, const BatchRasterState& rasterState = {});

    // Puts the rasterizer into one state outright, assuming nothing about what it was in.
    void ApplyRasterState(const RasterState& state);

    PipelineConfiguration& GetPipelineConfiguration();

    Graphics::ITexture* GetPositionTexture();
}

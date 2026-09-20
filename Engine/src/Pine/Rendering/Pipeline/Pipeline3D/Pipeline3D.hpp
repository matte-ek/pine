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
    // writes is pushed. The two travel together because the geometry that wants culling switched
    // off is the same geometry that then needs a bias - see SHADOW_SEPARATION_SLOPE_BIAS.
    struct RasterState
    {
        // False switches culling off entirely, and leaves FaceCulling meaning nothing.
        bool CullFaces = true;

        Graphics::FaceCullMode FaceCulling = Graphics::FaceCullMode::Back;

        // Ignored wherever depth bias is switched off, which is every pass but the shadow one.
        float SlopeBias = 0.f;
        float DepthBias = 0.f;
    };

    // The two states RenderBatch draws a pass's list with.
    //
    // Face culling is the one piece of pipeline state a material can override per draw: a
    // MaterialRenderFace::Both run is drawn with culling switched off. Nothing reads that state
    // back out of the graphics API, so a pass says here what its own geometry is drawn with and
    // what a two-sided run gets instead, and RenderBatch moves between the two.
    struct BatchRasterState
    {
        // What the pass draws everything else with. RenderBatch puts the rasterizer into this
        // state before its first draw, so it is a statement rather than a promise.
        RasterState Default;

        // What a MaterialRenderFace::Both run is drawn with instead.
        //
        // Culling off is the whole of it outside a shadow view. Inside one the bias moves too: an
        // open surface has no far side for the depth test to hide behind, so a cascade - which
        // takes its separation from front-face culling and renders at no bias at all - gets none
        // for such a surface and it shadows itself. ShadowPass fills in the pair terrain already
        // uses, for the same reason.
        //
        // Initialised positionally because C++17 has no designated initialisers; the fields are
        // RasterState's, immediately above.
        RasterState TwoSided = { false, Graphics::FaceCullMode::Back, 0.f, 0.f };
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
    //
    // 'rasterState' says what this pass draws with, so that a material asking for both of its
    // faces can be given a state of its own and the pass's put back after it. RenderBatch applies
    // the default half itself before drawing anything, so the pass does not have to set it twice.
    void RenderBatch(const Rendering::DrawList& drawList, const BatchRasterState& rasterState = {});

    // Puts the rasterizer into one state outright, assuming nothing about what it was in.
    //
    // Exposed for the same reason RenderBatch is: the shadow pass draws terrain between its
    // batches and has to move the rasterizer in and out of a state of its own to do it. Using this
    // rather than the graphics API directly keeps that draw described the same way the batch's
    // two-sided runs are.
    void ApplyRasterState(const RasterState& state);

    PipelineConfiguration& GetPipelineConfiguration();

    Graphics::ITexture* GetPositionTexture();
}

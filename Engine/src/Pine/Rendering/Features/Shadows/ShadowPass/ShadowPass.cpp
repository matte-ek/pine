#include "ShadowPass.hpp"

#include <cassert>

#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Shader/Shader.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Performance/Performance.hpp"
#include "Pine/Rendering/Features/Shadows/ShadowAtlas/ShadowAtlas.hpp"
#include "Pine/Rendering/Features/Shadows/ShadowView/ShadowView.hpp"
#include "Pine/Rendering/Features/TerrainRenderer/TerrainRenderer.hpp"
#include "Pine/Rendering/Pipeline/Pipeline3D/Pipeline3D.hpp"
#include "Pine/Rendering/Renderer3D/Renderer3D.hpp"
#include "Pine/Rendering/SceneProcessor/SceneProcessor.hpp"

using namespace Pine;

namespace
{
    Shader* m_ShadowShader = nullptr;

    // Scratch draw list, rebuilt for each view.
    Rendering::DrawList m_DrawList;

    // Raster states for open geometry, which has no far side for front-face culling to record and
    // so always renders at the SHADOW_SEPARATION_* bias pair. A two-sided material culls nothing;
    // terrain keeps its front faces.
    Pipeline3D::RasterState TwoSidedState()
    {
        Pipeline3D::RasterState state;

        state.CullFaces = false;
        state.SlopeBias = Rendering::SHADOW_SEPARATION_SLOPE_BIAS;
        state.DepthBias = Rendering::SHADOW_SEPARATION_DEPTH_BIAS;

        return state;
    }

    Pipeline3D::RasterState TerrainState()
    {
        Pipeline3D::RasterState state;

        // Explicit, because the surrounding view may be culling front faces.
        state.FaceCulling = Graphics::FaceCullMode::Back;
        state.SlopeBias = Rendering::SHADOW_SEPARATION_SLOPE_BIAS;
        state.DepthBias = Rendering::SHADOW_SEPARATION_DEPTH_BIAS;

        return state;
    }
}

void Rendering::ShadowPass::Setup()
{
    m_ShadowShader = Assets::Get<Shader>("engine/shaders/3d/shadow");

    assert(m_ShadowShader != nullptr);
}

void Rendering::ShadowPass::Render(const ShadowView* views, const int count, const ObjectBatchData& batchData)
{
    PINE_PF_SCOPE();

    auto* graphicsApi = Graphics::GetGraphicsAPI();
    auto& renderSettings = Renderer3D::GetRenderConfiguration();

    ShadowAtlas::GetFrameBuffer()->Bind();

    graphicsApi->SetDepthTestEnabled(true);
    graphicsApi->SetDepthBiasEnabled(true);

    Renderer3D::FrameReset();
    Renderer3D::UseRenderingContext(nullptr);

    renderSettings.OverrideShader = m_ShadowShader;
    renderSettings.IgnoreShaderVersions = true;
    renderSettings.SkipMaterialInitialization = true;

    // Scissor, because glViewport does not restrict glClear, and clearing the whole atlas would
    // wipe the cached tiles.
    graphicsApi->SetScissorEnabled(true);

    for (int i = 0; i < count; i++)
    {
        const auto& view = views[i];

        if (!view.NeedsRender)
        {
            continue;
        }

        const auto viewport = Vector2i(view.Viewport.x, view.Viewport.y);
        const auto size = Vector2i(view.Viewport.z, view.Viewport.w);

        graphicsApi->SetViewport(viewport, size);
        graphicsApi->SetScissor(viewport, size);
        graphicsApi->ClearBuffers(Graphics::DepthBuffer);

        Renderer3D::SetViewProjection(view.ViewProjection);

        // RenderBatch applies these itself, so nothing is set on the graphics API here.
        Pipeline3D::BatchRasterState rasterState;

        rasterState.Default.FaceCulling = view.FaceCulling;
        rasterState.Default.SlopeBias = view.SlopeBias;
        rasterState.Default.DepthBias = view.DepthBias;

        rasterState.TwoSided = TwoSidedState();

        // Opaque and Discard both cast; leaving out Discard would stop foliage casting. Batched
        // rather than sorted front to back: a depth-only pass gains little from the sort.
        m_DrawList.Build(batchData.OpaqueObjects, MaterialRenderingMode::Opaque, view.Visibility, { Rendering::DrawOrder::Batched });
        Pipeline3D::RenderBatch(m_DrawList, rasterState);

        m_DrawList.Build(batchData.OpaqueObjects, MaterialRenderingMode::Discard, view.Visibility, { Rendering::DrawOrder::Batched });
        Pipeline3D::RenderBatch(m_DrawList, rasterState);

        // Terrain is not in the batch and culls its own chunks against the view. It is drawn with
        // TerrainState rather than the view's state: under a cascade's front-face culling only
        // the chunk skirts would write depth.
        Pipeline3D::ApplyRasterState(TerrainState());

        // Without skirts, which would cast a wall's shadow along every chunk edge. See
        // TerrainView::DrawSkirts.
        TerrainRenderer::Render({ view.ViewFrustum, view.Origin, nullptr, false });

        Pipeline3D::ApplyRasterState(rasterState.Default);

        if (view.AtlasSlot >= 0)
        {
            ShadowAtlas::MarkRendered(view.AtlasSlot);
        }
    }

    graphicsApi->SetScissorEnabled(false);
    graphicsApi->SetDepthBiasEnabled(false);
    graphicsApi->SetDepthBias(0.f, 0.f);
    graphicsApi->SetFaceCullingMode(Graphics::FaceCullMode::Back);

    renderSettings.OverrideShader = nullptr;
    renderSettings.IgnoreShaderVersions = false;
    renderSettings.SkipMaterialInitialization = false;
}

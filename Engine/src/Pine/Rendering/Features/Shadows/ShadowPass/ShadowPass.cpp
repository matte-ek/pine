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

    // Scratch storage for the draw list built per view below. Shadow views are rendered one after
    // another and each submits before the next builds, so one list serves them all.
    Rendering::DrawList m_DrawList;

    // The two kinds of open geometry a shadow view draws: a material asking for both of its faces,
    // and terrain. Neither has a far side for the depth test to hide behind, so neither can take
    // its separation from the cascades' front-face culling the way a closed mesh does, and both
    // pay for it with SHADOW_SEPARATION_* instead - the pair a local view renders everything at.
    //
    // Culling is the only thing they disagree on, and it is the point of each: a leaf card is
    // drawn from both sides, and a height field has one front face per column worth keeping.
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

        // Stated rather than left at the struct default, because the view around it may well be
        // culling the other way and that is exactly what terrain cannot do.
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

    // Scissor, not just viewport: glViewport does not restrict glClear, so without this the only
    // way to clear one tile would be to clear the whole atlas - which would wipe every other
    // tile, and those tiles are exactly what the cache is keeping.
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

        // The one statement of what this view rasterizes with. RenderBatch applies the default
        // half before it draws anything, so there is nothing to set on the graphics API here -
        // and the terrain draw below reaches for the same two states rather than restating their
        // values a second time.
        Pipeline3D::BatchRasterState rasterState;

        rasterState.Default.FaceCulling = view.FaceCulling;
        rasterState.Default.SlopeBias = view.SlopeBias;
        rasterState.Default.DepthBias = view.DepthBias;

        rasterState.TwoSided = TwoSidedState();

        // Both modes, matching what the old whole-batch draw did: it ignored the material
        // rendering mode entirely, so alpha-tested geometry cast a solid shadow. A draw list is
        // built per mode, so leaving out the Discard one would silently stop foliage casting.
        //
        // Batched rather than front to back, although this pass writes nothing but depth: a view
        // is rendered once per frame at most and often reused from the cache, so the sort would be
        // paid on every view for a saving on the one pass in the frame that does no shading.
        m_DrawList.Build(batchData.OpaqueObjects, MaterialRenderingMode::Opaque, view.Visibility, { Rendering::DrawOrder::Batched });
        Pipeline3D::RenderBatch(m_DrawList, rasterState);

        m_DrawList.Build(batchData.OpaqueObjects, MaterialRenderingMode::Discard, view.Visibility, { Rendering::DrawOrder::Batched });
        Pipeline3D::RenderBatch(m_DrawList, rasterState);

        // Terrain is not in the batch, so it needs its own call or the ground casts nothing.
        // It culls its own chunks against this view rather than reading view.Visibility, which
        // cannot hold them - see RenderCulling::VisibilitySet. No statistics sink: the atlas is
        // drawn once for the whole scene and belongs to no rendering context.
        //
        // Drawn with its own culling and bias rather than the view's, because a height field is
        // single-sided: it has one surface per column and no far side at all. A cascade culls
        // front faces to buy its separation for free, which for terrain would discard the
        // ground itself and leave only the chunk skirts writing depth. Back faces culled and an
        // explicit bias pair instead - the same trade a local view already makes, and for the
        // same reason its comment gives.
        Pipeline3D::ApplyRasterState(TerrainState());

        // Without skirts: they hang below the surface to hide a crack between detail levels,
        // and a vertical rim at every chunk edge writing depth casts a wall's shadow across the
        // ground next to it. See TerrainView::DrawSkirts.
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

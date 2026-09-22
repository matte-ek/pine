#pragma once

#include "Pine/Core/Math/Frustum/Frustum.hpp"
#include "Pine/Graphics/Interfaces/IGraphicsAPI.hpp"
#include "Pine/Rendering/Features/RenderCulling/RenderCulling.hpp"

namespace Pine::Rendering
{
    // The bias pair for depth separation that cannot come from front-face culling: every local
    // light view, and inside any view, open geometry (terrain and two-sided materials). One pair
    // for all of them, because it is one problem - tune them together.
    constexpr float SHADOW_SEPARATION_SLOPE_BIAS = 2.f;
    constexpr float SHADOW_SEPARATION_DEPTH_BIAS = 4.f;

    // One depth render of the scene from one projection, into one region of one render target.
    //
    // Every shadow source decomposes into these: a directional light is CASCADE_COUNT views, a spot
    // light one, a point light six cube faces. The build/cull/render loop therefore has no
    // per-light-type branch.
    struct ShadowView
    {
        Matrix4f ViewProjection = Matrix4f(1.f);

        // Always FromViewProjection(ViewProjection), cached because extraction is not free.
        Frustum ViewFrustum;

        // x, y, width, height in target texels.
        Vector4i Viewport = Vector4i(0);

        // Where this view looks from, in world space, for distance-based level of detail (terrain
        // chunks). A cascade has no single origin and uses the scene camera's position, so terrain
        // casts with the same detail level the main pass draws it at.
        Vector3f Origin = Vector3f(0.f);

        // World size of one of this view's texels, per unit distance from the view origin:
        // 2 * tan(fov/2) / Viewport.z. Multiply by a point's distance from the origin and you have
        // the world footprint of the texel covering it.
        //
        // Perspective views only; cascades leave this at zero.
        float TexelWorldScale = 0.f;

        // Atlas slot backing this view, or -1 for a view that renders somewhere else.
        int AtlasSlot = -1;

        // Constant and slope-scaled depth offset applied while rendering this view. Zero for the
        // cascades, which avoid acne with front-face culling instead.
        float DepthBias = 0.f;
        float SlopeBias = 0.f;

        // Which faces the rasterizer discards while drawing this view. Cascades cull front faces,
        // which hides acne without bias. Local lights cull back faces, because at short range
        // front-face culling peter-pans visibly, and pay for it with the bias pair above.
        Graphics::FaceCullMode FaceCulling = Graphics::FaceCullMode::Back;

        // Whether the target region has to be redrawn this frame. False means what is already there
        // is still correct: the view is still sampled, it just costs nothing to render. Set by
        // whoever built the view.
        bool NeedsRender = true;

        // Which objects this view draws. Owned per view because several views are live at once.
        RenderCulling::VisibilitySet Visibility;
    };
}

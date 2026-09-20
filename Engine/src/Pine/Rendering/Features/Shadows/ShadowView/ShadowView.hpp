#pragma once

#include "Pine/Core/Math/Frustum/Frustum.hpp"
#include "Pine/Graphics/Interfaces/IGraphicsAPI.hpp"
#include "Pine/Rendering/Features/RenderCulling/RenderCulling.hpp"

namespace Pine::Rendering
{
    // The bias pair a shadow view renders with when its depth separation cannot come from
    // front-face culling.
    //
    // A cascade gets that separation for free: it culls front faces, so what it records is the far
    // side of a caster and the near side it shadows sits a whole thickness in front of it. It needs
    // no bias at all. Everything else has to buy the same separation explicitly with this pair -
    //
    // - a local light view, which culls back faces like the scene pass and so renders *everything*
    //   at this bias (see BuildSpotView), and
    // - inside any view, geometry with no far side to hide behind: terrain, and a material asking
    //   for both of its faces (see ShadowPass::Render).
    //
    // One pair rather than one per case, because it is one problem. Tuning it for a local light and
    // leaving foliage on a different number would be a bug, not a choice.
    constexpr float SHADOW_SEPARATION_SLOPE_BIAS = 2.f;
    constexpr float SHADOW_SEPARATION_DEPTH_BIAS = 4.f;

    // One depth render of the scene from one projection, into one region of one render target.
    //
    // Every shadow source decomposes into some number of these and nothing else: a directional light
    // is CASCADE_COUNT views, a spot light is one, a point light is six cube faces. That is the whole
    // point of the type - the build/cull/render loop has no per-light-type branch in it, because by
    // the time it runs there are no light types left, only views.
    //
    // Deliberately not called a cascade, a face, or an atlas tile: those are the three things this
    // one type replaced, and naming it after any of them would invite the branch back.
    //
    // The shape is more general than shadows - a reflection probe's six faces or an offline lightmap
    // bake would want the same "a projection, its frustum, a slice of a render target, and who is
    // visible in it", and the bias pair is the only field that is shadow-specific. It lives under
    // Features/Shadows/ because shadows are its only user today. A second one is a reason to move it
    // up to Rendering/, not a reason to have put it there first.
    struct ShadowView
    {
        Matrix4f ViewProjection = Matrix4f(1.f);

        // Always FromViewProjection(ViewProjection). Kept alongside rather than derived at each use
        // because culling asks for it once per view per frame and extraction is not free.
        Frustum ViewFrustum;

        // x, y, width, height in target texels.
        Vector4i Viewport = Vector4i(0);

        // Where this view looks from, in world space. Read by anything whose level of detail is a
        // distance - terrain chunks today.
        //
        // A spot or point view sits at its light and says so. A cascade is orthographic and has no
        // single origin, and carries the scene camera's position instead: that is the point its box
        // is built around, and the point the main pass picks its terrain levels from, so a chunk
        // casts the shadow of the silhouette it is actually drawn with rather than a coarser one.
        Vector3f Origin = Vector3f(0.f);

        // World size of one of this view's texels, per unit distance from the view origin:
        // 2 * tan(fov/2) / Viewport.z. Multiply by a point's distance from the origin and you have
        // the world footprint of the texel covering it.
        //
        // Perspective views only. An orthographic view's texels are the same size everywhere, so
        // there is nothing to scale by distance and the cascades leave this at zero. Stored rather
        // than recovered from ViewProjection because the projection is built here and the factors
        // that go into it are not separable again afterwards.
        float TexelWorldScale = 0.f;

        // Atlas slot backing this view, or -1 for a view that renders somewhere else. Kept so a view
        // can report back to the allocator.
        int AtlasSlot = -1;

        // Constant and slope-scaled depth offset applied while rendering this view. Zero for the
        // cascades, which avoid acne with front-face culling instead.
        float DepthBias = 0.f;
        float SlopeBias = 0.f;

        // Which faces the rasterizer discards while drawing this view.
        //
        // A property of the view rather than of the pass, because the two kinds of shadow view
        // genuinely disagree and now share a pass. A cascade culls front faces: the light is
        // effectively infinitely far away and the scene is closed-ish, so recording back faces
        // hides acne for free. A local light culls back faces instead - at the short range one works
        // over, front-face culling peter-pans visibly and falls apart on single-sided geometry - and
        // pays for it with the bias pair above.
        Graphics::FaceCullMode FaceCulling = Graphics::FaceCullMode::Back;

        // Whether the target region's contents are stale and have to be redrawn this frame.
        //
        // False means "what is already in the target is still correct" - the view is still live and
        // still sampled, it just costs nothing. A view whose projection and visible set are both
        // unchanged since it was last drawn produces identical pixels, and in a static scene that is
        // almost every view almost every frame.
        //
        // Deciding this is the caller's job, not the view's: only whoever built the projection knows
        // what its inputs were. A reflection probe would answer it from a different set of inputs
        // than a shadow does, which is why the flag is here and the policy is not.
        bool NeedsRender = true;

        // Which objects this view draws. Visibility is a property of (object, frustum) rather than
        // of the object, which is why it is owned here and not on the component - several views are
        // live at once and they disagree.
        RenderCulling::VisibilitySet Visibility;
    };
}

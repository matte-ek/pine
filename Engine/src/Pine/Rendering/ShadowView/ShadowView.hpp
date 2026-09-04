#pragma once

#include "Pine/Core/Math/Frustum/Frustum.hpp"
#include "Pine/Graphics/Interfaces/IGraphicsAPI.hpp"
#include "Pine/Rendering/Features/RenderCulling/RenderCulling.hpp"

namespace Pine::Graphics
{
    class IFrameBuffer;
}

namespace Pine::Rendering
{
    // One depth render of the scene from one projection, into one region of one render target.
    //
    // Every shadow source decomposes into some number of these and nothing else: a directional light
    // is CASCADE_COUNT views, a spot light is one, a point light is six cube faces. That is the whole
    // point of the type - the build/cull/render loop has no per-light-type branch in it, because by
    // the time it runs there are no light types left, only views.
    //
    // Deliberately not called a cascade, a face, or an atlas tile, and deliberately not living under
    // Features/Shadows/: a reflection probe's six faces, a light probe bake or an offline lightmap
    // rasteriser all want "a projection, its frustum, a slice of a render target, and who is visible
    // in it", and none of them would want to rename this to use it. The bias pair is the only field
    // that is shadow-specific, and it is ignorable.
    struct ShadowView
    {
        Matrix4f ViewProjection = Matrix4f(1.f);

        // Always FromViewProjection(ViewProjection). Kept alongside rather than derived at each use
        // because culling asks for it once per view per frame and extraction is not free.
        Frustum ViewFrustum;

        // Where this view renders. A (framebuffer, layer, rect) triple rather than an "atlas tile":
        // cascades render into a layer of their own array texture, an atlas is many rects in one
        // framebuffer, and an offline bake would be a third target shape. All three are this.
        //
        // TargetLayer is -1 for a non-array target, in which case the whole texture is attached.
        Graphics::IFrameBuffer* Target = nullptr;
        int TargetLayer = -1;

        // x, y, width, height in target texels.
        Vector4i Viewport = Vector4i(0);

        // Atlas slot backing this view, or -1 for a view that renders somewhere else (the cascades
        // render into their own array texture). Kept so a view can report back to the allocator.
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

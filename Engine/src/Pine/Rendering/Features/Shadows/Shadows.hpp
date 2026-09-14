#pragma once
#include <vector>

#include "Pine/Rendering/SceneProcessor/SceneProcessor.hpp"

namespace Pine
{
    namespace Graphics
    {
        class ITexture;
    }

    namespace Rendering
    {
        struct ObjectBatchData;
    }

    class Light;
    class Camera;
}

namespace Pine::Rendering::Shadows
{
    void Setup();
    void Shutdown();

    void NewFrame(Camera* sceneCamera);

    // Per-frame statistics, for the debug panel. Local shadow views render at scene level rather
    // than per rendering context, so their cost has no RenderingContext to be counted against.
    struct Statistics
    {
        int LocalViewCount = 0;

        // Local only, so that this and TilesCached partition LocalViewCount and the two can be read
        // against each other. Cascades are counted by CascadeViewCount instead: they render per
        // rendering context rather than per frame, so folding them in here would make a local cache
        // hit rate rise and fall with how many viewports happen to be open.
        int TilesRendered = 0;

        // Views that were live and sampled but cost nothing, because what was already in their tile
        // was still correct. In a static scene this should be every one of them.
        int TilesCached = 0;

        int CastersDrawn = 0;

        // Cascades render inside each rendering context's prepass, so with an editor viewport and a
        // game camera both live these count every cascade rendered this frame, not per viewer. Every
        // cascade view always renders, so this is a tile count as well as a view count.
        int CascadeViewCount = 0;
        int CascadeCastersDrawn = 0;

        void Reset()
        {
            LocalViewCount = 0;
            TilesRendered = 0;
            TilesCached = 0;
            CastersDrawn = 0;
            CascadeViewCount = 0;
            CascadeCastersDrawn = 0;
        }
    };

    // Allocates atlas tiles and builds a ShadowView for every local light that is casting this
    // frame, then renders them.
    //
    // Called once per frame from Pipeline3D::Prepare, NOT per rendering context: a spot or point
    // light's shadow map does not depend on the viewer at all, so rendering it once per context
    // (editor viewport plus game camera) would be pure duplicated work. Cascades are the opposite -
    // they are built from the camera frustum - which is why they stay in the per-context prepass.
    void PrepareLocalViews(const SceneProcessor::SceneProcessorContext& sceneContext);
    void RenderLocalViews(const ObjectBatchData& batchData);

    // Drops every local view and releases every tile. Call instead of PrepareLocalViews when shadows
    // are off: a light still pointing at the view it held when they were switched on keeps sampling
    // a tile nothing refreshes any more, so its shadow freezes in place rather than disappearing.
    void ClearLocalViews(const std::vector<Light*>& lights);

    const Statistics& GetStatistics();

    // What the selection policy and the cache decided about one atlas tile, for the debug panel.
    // Both are invisible in the final image when they work and obvious here when they don't.
    struct TileDebugInfo
    {
        bool Active = false;
        float Importance = 0.f;
        float Fade = 0.f;
        float ResidencyTime = 0.f;
        bool ContentValid = false;

        // How many tiles the owning light holds in total: 1 for a spot, 6 for a point light.
        int TileCount = 0;

        // The tile is a pinned reservation rather than a light's claim, so its owner is not a Light
        // and must not be treated as one.
        bool Reserved = false;
    };

    TileDebugInfo GetTileDebugInfo(int slot);

    // Builds and renders the directional cascades for the *current* rendering context.
    //
    // Per context rather than per frame, unlike the local views: a cascade is derived from the
    // camera frustum, so an editor viewport and a game camera need different ones. Each context
    // renders its cascades immediately before it draws, into the same pinned tiles.
    void RenderPassLight(Light* light, const SceneProcessor::SceneProcessorContext& sceneContext);
}

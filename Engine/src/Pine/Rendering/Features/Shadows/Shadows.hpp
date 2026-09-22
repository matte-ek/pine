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

    // Per-frame statistics, for the debug panel.
    struct Statistics
    {
        int LocalViewCount = 0;

        // Local views only, so this and TilesCached partition LocalViewCount.
        int TilesRendered = 0;

        // Live views whose tile was still correct, so they cost nothing.
        int TilesCached = 0;

        int CastersDrawn = 0;

        // Summed over every rendering context this frame. Cascades always render.
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

    // Allocates atlas tiles and builds a ShadowView for every local light casting this frame;
    // RenderLocalViews then draws the stale ones. Called once per frame from Pipeline3D::Prepare,
    // not per rendering context, because a local light's shadow does not depend on the viewer.
    void PrepareLocalViews(const SceneProcessor::SceneProcessorContext& sceneContext);
    void RenderLocalViews(const ObjectBatchData& batchData);

    // Drops every local view and releases every unpinned tile. Call instead of PrepareLocalViews
    // when shadows are off, or lights keep sampling tiles nothing refreshes.
    void ClearLocalViews(const std::vector<Light*>& lights);

    const Statistics& GetStatistics();

    // What the selection policy and the cache decided about one atlas tile, for the debug panel.
    struct TileDebugInfo
    {
        bool Active = false;
        float Importance = 0.f;
        float Fade = 0.f;
        float ResidencyTime = 0.f;
        bool ContentValid = false;

        // How many tiles the owning light holds in total: 1 for a spot, 6 for a point light.
        int TileCount = 0;

        // A pinned reservation, whose owner is not a Light.
        bool Reserved = false;
    };

    TileDebugInfo GetTileDebugInfo(int slot);

    // Builds and renders the directional cascades for the current rendering context. Per context
    // rather than per frame, because a cascade is fitted to the camera frustum. Each context
    // renders into the same pinned tiles immediately before it draws.
    void RenderPassLight(Light* light, const SceneProcessor::SceneProcessorContext& sceneContext);
}

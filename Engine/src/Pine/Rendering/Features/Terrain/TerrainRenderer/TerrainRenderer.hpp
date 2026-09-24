#pragma once
#include "Pine/Core/Math/Frustum/Frustum.hpp"

#include <optional>
#include "Pine/Rendering/RenderingContext.hpp"
#include "Pine/World/Components/TerrainRenderer/TerrainRendererComponent.hpp"

namespace Pine::Rendering::SceneProcessor
{
    struct SceneProcessorContext;
}

namespace Pine::Rendering::TerrainRenderer
{
    // The viewer a pass is drawing terrain for. Passed in rather than stored, because several
    // viewers draw the same terrain each frame - viewports and shadow views alike.
    struct TerrainView
    {
        // Chunks that do not touch it are skipped.
        const Frustum& ViewFrustum;

        // Distance from here picks each chunk's detail level. See ShadowView::Origin for what a
        // shadow view passes.
        Vector3f LodOrigin;

        // Where the chunk counters land, or null for a pass that reports none, such as a shadow
        // view.
        RenderingStatistics* Statistics = nullptr;

        // Whether the chunk skirts are drawn. A skirt is a vertical rim below a chunk's edge that
        // hides the crack between neighbours at different detail levels. Shadow views leave them
        // out: a skirt would cast a wall's shadow, where the crack only lets a thin seam of light
        // through.
        bool DrawSkirts = true;
    };

    // Where the editor's sculpting brush is sitting, for the overlay drawn on the ground under it.
    struct BrushOverlay
    {
        // Only this terrain draws the overlay.
        UId Terrain;

        // Terrain-local, in world units, matching the coordinate the brush itself works in.
        Vector2f Centre{};

        float Radius = 0.f;

        // Width of the brighter band drawn at the rim, in world units.
        float RingWidth = 0.5f;
    };

    // Sets or clears the overlay. Editor-only, and one for the whole renderer, so a Game viewport
    // beside the Level viewport shows the ring too.
    void SetBrushOverlay(const std::optional<BrushOverlay>& overlay);
    const std::optional<BrushOverlay>& GetBrushOverlay();

    void Setup();
    void Shutdown();

    // Brings every terrain up to date with the scene: the chunk meshes its height field implies,
    // and the light slots its chunks are lit through. Writes back whether any terrain changed, for
    // the shadow tile cache. Call once per frame, before any pass renders.
    void Prepare(SceneProcessor::SceneProcessorContext& sceneContext);

    // Clears this context's terrain chunk counters, so they describe one pass rather than however
    // many passes have run into them this frame. Called by the pipeline at the start of each pass
    // that draws terrain.
    void BeginPass(RenderingContext& context);

    // Draws the chunks of every terrain in the world that 'view' can see, each at the detail level
    // its distance from that view earns.
    void Render(const TerrainView& view);
}

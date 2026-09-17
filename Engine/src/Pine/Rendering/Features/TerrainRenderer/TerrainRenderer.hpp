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
    // The viewer a pass is drawing terrain for.
    //
    // A parameter rather than state on this feature because every decision in it belongs to the
    // viewer and not to the terrain: an editor viewport, a game camera and a spot light's shadow
    // view all draw the same terrain in the same frame and want different answers from it. A
    // terrain that remembered "the" camera gave the second viewer the first one's.
    //
    // It is not a RenderingContext because a shadow view is not one - it has a frustum and a
    // target, and none of the camera, framebuffer or skybox a context carries.
    struct TerrainView
    {
        // Chunks that do not touch it are skipped.
        const Frustum& ViewFrustum;

        // Distance from here picks each chunk's detail level: the camera for a rendering context,
        // the light for a local shadow view, the camera again for a cascade - which is what makes
        // a chunk cast the shadow of the silhouette it is actually drawn with.
        Vector3f LodOrigin;

        // Where the chunk counters land, or null for a pass that reports none. The shadow atlas is
        // drawn once for the whole scene and so belongs to no single context's statistics.
        RenderingStatistics* Statistics = nullptr;

        // Whether the chunk skirts are drawn. A skirt is a vertical rim hanging below a chunk's
        // edge whose only job is to hide the crack between two neighbours drawn at different detail
        // levels - so it belongs in any pass that has to agree with what is on screen, and in no
        // pass that decides what light reaches the ground. A shadow view that draws them gets a
        // wall at every chunk edge, shadowing the ground beside it.
        //
        // The cost of leaving them out is the crack they were hiding, now in the shadow map rather
        // than on screen: a thin seam where two neighbours at different levels meet, which lets a
        // little light through. A seam is a far better trade than a wall.
        bool DrawSkirts = true;
    };

    // Where the editor's sculpting brush is sitting, for the overlay drawn on the ground under it.
    struct BrushOverlay
    {
        // Only this terrain draws the overlay. Without it a second terrain in the scene would get a
        // ring of its own at the same terrain-local coordinate, which is not where the cursor is.
        UId Terrain;

        // Terrain-local, in world units, matching the coordinate the brush itself works in.
        Vector2f Centre{};

        float Radius = 0.f;

        // Width of the brighter band drawn at the rim, in world units.
        float RingWidth = 0.5f;
    };

    // Sets or clears the overlay. Editor-only: nothing calls this in a built game, so the shader
    // variant that draws it is never compiled there.
    //
    // One overlay for the whole renderer rather than one per rendering context, because there is
    // one cursor. A Game viewport open beside the Level viewport therefore shows the ring too,
    // which is a cosmetic oddity in an editor-only path and not worth a field on every context.
    void SetBrushOverlay(const std::optional<BrushOverlay>& overlay);
    const std::optional<BrushOverlay>& GetBrushOverlay();

    void Setup();
    void Shutdown();

    // Brings every terrain up to date with the scene: the chunk meshes its height field implies,
    // and the light slots its chunks are lit through. Writes back whether any terrain changed, for
    // the shadow tile cache.
    //
    // Called once per frame before any pass renders, rather than from inside one: the depth
    // pre-pass used to be what generated the meshes, which put an unbounded amount of work in the
    // middle of a pass, and light slots are scene state that every viewer shares.
    void Prepare(SceneProcessor::SceneProcessorContext& sceneContext);

    // Clears this context's terrain chunk counters, so they describe one pass rather than however
    // many passes have run into them this frame. Called by the pipeline at the start of each pass
    // that draws terrain.
    void BeginPass(RenderingContext& context);

    // Draws the chunks of every terrain in the world that 'view' can see, each at the detail level
    // its distance from that view earns.
    void Render(const TerrainView& view);
}

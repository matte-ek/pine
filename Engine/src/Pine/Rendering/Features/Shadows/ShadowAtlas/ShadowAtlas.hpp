#pragma once

#include <cstdint>
#include <vector>

#include "Pine/Core/Math/Math.hpp"

namespace Pine::Graphics
{
    class IFrameBuffer;
    class ITexture;
}

namespace Pine::Rendering::ShadowAtlas
{
    // One depth texture partitioned into tiles, handed out to shadow views.
    //
    // Chosen over a cube map array because it gives per-light resolution, one sampler, and one
    // allocator shared by spots, point faces and eventually cascades - and, most importantly,
    // because a light can keep the *same* tile across frames, which is the precondition for ever
    // caching a shadow render instead of redoing it.
    //
    // The tax it charges over a cube array is manual cube-face selection and seam handling in the
    // shader. That bill comes due with point lights, not here.

    // Named by what they are - a fraction of the atlas edge - rather than by how big they feel or
    // by who asked for them. At the default 4096 atlas: 2048, 1024, 512.
    //
    // There used to be a 1/16 class with a whole quadrant of 64 tiles behind it. It was a quarter of
    // the atlas that could never be used: SHADOW_VIEW_COUNT caps the engine at 32 live views, so 64
    // tiles of anything is unreachable by construction.
    enum class TileSize
    {
        Half,
        Quarter,
        Eighth
    };

    struct Slot
    {
        Vector4i Rect = Vector4i(0);
        TileSize Size = TileSize::Quarter;

        // Whoever last claimed this tile. Opaque on purpose: the allocator has no business knowing
        // what a light is.
        const void* Owner = nullptr;

        std::uint64_t LastClaimedFrame = 0;

        bool RenderedThisFrame = false;

        // Exempt from the sweep. See Reserve.
        bool Pinned = false;
    };

    void Setup();
    void Shutdown();

    // Call once per frame before any Acquire. Pairs with EndFrame, which releases every tile that
    // was not re-claimed - that mark-and-sweep is what makes a tile stable for as long as its owner
    // keeps asking for it, and free the moment it stops.
    void BeginFrame();
    void EndFrame();

    // Grants 'count' tiles of one size to 'owner' and writes their indices to 'outSlots', reusing
    // the ones it already held where possible. Returns false and grants nothing if the whole group
    // cannot be satisfied.
    //
    // All or nothing, because a caller asking for several tiles needs all of them to be useful: a
    // point light holding four of its six cube faces is not two-thirds shadowed, it has a hard
    // discontinuity along every edge between a face it got and one it didn't. Nothing is mutated
    // until the whole group is known to be available, so the failure path has nothing to undo.
    //
    // Slots come back in ascending index order, which is what makes the mapping from a caller's
    // sub-index (cube face 0..5) to a tile stable across frames - and tile stability is the whole
    // precondition for caching what is drawn in them.
    //
    // Deliberately not a general packer: at the sizes this budget uses there is nothing to pack,
    // and the interesting question is which lights win tiles, not how to arrange them.
    bool Acquire(TileSize size, const void* owner, int count, int* outSlots);

    // Whether 'owner' holds any tile at all. The allocator's sweep is the authority on that, so
    // anything keeping its own per-owner bookkeeping has to be able to ask.
    bool HasTiles(const void* owner);

    // Takes 'count' tiles out of circulation permanently and gives them to 'owner'.
    //
    // A pinned tile is exempt from the sweep, so its owner never has to re-claim it and can never
    // lose it. That is the exact opposite of how everything else here works, and it is deliberate:
    // mark-and-sweep is right for a contended resource where not asking means you stopped caring,
    // and wrong for one whose owner is structural. Directional cascades are the case - they exist
    // whenever the level has a sun, they are the same tiles every frame, and a frame in which they
    // lost a contest for space would simply be a frame with no sun shadows.
    //
    // Intended for setup-time reservations, so the cost is paid once and visibly: the tiles are gone
    // from the pool for the process lifetime whether or not anything ever renders into them.
    bool Reserve(TileSize size, const void* owner, int count, int* outSlots);

    Vector4i GetViewport(int slot);

    // The slot's rect in normalized atlas UV, for the fragment shader.
    Vector4f GetUvRect(int slot);

    void MarkRendered(int slot);

    Graphics::IFrameBuffer* GetFrameBuffer();
    Graphics::ITexture* GetTexture();
    int GetResolution();

    const std::vector<Slot>& GetSlots();

    // Artificially caps how many tiles may be claimed per frame, regardless of how many exist.
    //
    // An allocator that never runs out is an allocator whose exhaustion path has never run, and that
    // bug surfaces in a large level at the worst possible time.
    //
    // -1 disables the cap; 0 and up are real caps, and 0 (no tiles at all) is the setting that
    // proves the control is connected. The cap applies to lights that already hold a tile too, so
    // lowering it evicts rather than merely refusing newcomers.
    void SetDebugSlotLimit(int limit);
    int GetDebugSlotLimit();
}

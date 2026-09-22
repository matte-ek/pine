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
    // One depth texture partitioned into tiles, handed out to shadow views. A light keeps the same
    // tile across frames for as long as it keeps claiming it, which is what lets Shadows cache
    // what is drawn there.

    // A fraction of the atlas edge. At the default 4096 atlas: 2048, 1024, 512.
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

        // Whoever last claimed this tile. Usually a Light, but a pinned tile's owner is whatever
        // token its reserver passed.
        const void* Owner = nullptr;

        std::uint64_t LastClaimedFrame = 0;

        bool RenderedThisFrame = false;

        // Exempt from the sweep. See Reserve.
        bool Pinned = false;
    };

    void Setup();
    void Shutdown();

    // Call once per frame before any Acquire. EndFrame releases every tile that was not re-claimed
    // in between, so a tile stays with its owner for as long as it keeps asking.
    void BeginFrame();
    void EndFrame();

    // Grants 'count' tiles of one size to 'owner' and writes their indices to 'outSlots', reusing
    // the ones it already held where possible.
    //
    // All or nothing: returns false and grants nothing if the whole group cannot be satisfied. A
    // point light holding four of its six cube faces is not partly shadowed, it has a hard seam
    // along every edge between a face it got and one it did not.
    //
    // Slots come back in ascending index order, so a caller's sub-index (cube face 0..5) maps to
    // the same tile every frame.
    bool Acquire(TileSize size, const void* owner, int count, int* outSlots);

    // How many unpinned tiles of one class exist, free or not. A property of the layout, for
    // checking whether a class could ever serve a group of a given size.
    int GetTileCapacity(TileSize size);

    // Whether 'owner' holds any tile at all, for callers keeping their own per-owner bookkeeping.
    bool HasTiles(const void* owner);

    // Takes 'count' tiles out of circulation for the process lifetime and gives them to 'owner'.
    //
    // A pinned tile is exempt from the sweep, so its owner never re-claims it and can never lose
    // it. For owners that need the same tiles every frame without competing for them - the
    // directional cascades. Call at setup.
    bool Reserve(TileSize size, const void* owner, int count, int* outSlots);

    Vector4i GetViewport(int slot);

    // The slot's rect in normalized atlas UV, for the fragment shader.
    Vector4f GetUvRect(int slot);

    void MarkRendered(int slot);

    Graphics::IFrameBuffer* GetFrameBuffer();
    Graphics::ITexture* GetTexture();
    int GetResolution();

    const std::vector<Slot>& GetSlots();

    // Caps how many tiles may be claimed per frame, so the exhaustion path can be exercised on
    // purpose. -1 disables the cap; 0 denies every claim. It applies to lights already holding
    // tiles too, so lowering it evicts rather than only refusing newcomers.
    void SetDebugSlotLimit(int limit);
    int GetDebugSlotLimit();
}

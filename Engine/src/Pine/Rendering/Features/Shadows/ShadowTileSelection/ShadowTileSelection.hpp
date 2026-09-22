#pragma once

#include <vector>

#include "Pine/Rendering/Features/Shadows/ShadowAtlas/ShadowAtlas.hpp"

namespace Pine
{
    class Light;
}

// Decides which lights cast this frame, and at what resolution.
//
// All the hysteresis lives here: incumbency, the challenger margin, the minimum residency, the
// promotion and demotion thresholds, and the fade. They all exist because a tile that changes hands
// is cold and has to be fully re-rendered, and they should be tuned against each other.
namespace Pine::Rendering::ShadowTileSelection
{
    // One tile per cube face, the largest group any light can ask for.
    constexpr int POINT_FACE_COUNT = 6;
    constexpr int MAX_LIGHT_TILE_COUNT = POINT_FACE_COUNT;

    // A light eligible for tiles this frame, scored and ranked.
    struct ShadowCandidate
    {
        Light* LightPtr = nullptr;
        float Importance = 0.f;

        // What it costs: one tile for a spot, six for a point light.
        ShadowAtlas::TileSize Size = ShadowAtlas::TileSize::Quarter;
        int TileCount = 1;

        // Whether it held tiles coming into this frame.
        bool IsIncumbent = false;
        float ResidencyTime = 0.f;

        // How faded in its shadow was coming into this frame, 0 for a light that held nothing. A
        // candidate that loses keeps drawing while this is above zero, to fade out.
        float Fade = 0.f;
    };

    // What is remembered about one light's claim, shared by all of its tiles. Cache validity is
    // per tile instead, and lives in Shadows.
    struct LightShadowState
    {
        const void* Owner = nullptr;

        float ResidencyTime = 0.f;
        float Fade = 0.f;
        float Importance = 0.f;

        int TileCount = 0;

        // The size class its tiles currently come from, for the demotion threshold.
        ShadowAtlas::TileSize Size = ShadowAtlas::TileSize::Eighth;
    };

    // Ranks every light that could cast this frame, best first, and clears the shadow view index of
    // every light it is given - casting or not, so a light that loses its tiles this frame cannot
    // keep pointing at the view some other light now owns.
    const std::vector<ShadowCandidate>& SelectShadowCandidates(const std::vector<Light*>& lights);

    // Claims a candidate's group of tiles, demoting it to the smallest class if the class it asked
    // for has no room left this frame. Writes the size actually granted to 'outSize', and returns
    // false only when nothing could be granted at all.
    bool AcquireTiles(const ShadowCandidate& candidate, ShadowAtlas::TileSize* outSize, int* outSlots);

    // Advances the claim behind a granted candidate - residency, fade, importance, size - and hands
    // it back. Call once per candidate that was actually granted tiles, with the size it got rather
    // than the size it asked for, and with whether it won its place or is on its way out.
    const LightShadowState& RecordGrant(const ShadowCandidate& candidate, ShadowAtlas::TileSize grantedSize,
                                        bool wins, float deltaTime);

    // The claim held by 'owner', or nullptr. For the debug panel, which has an atlas slot's owner
    // and wants the numbers behind it.
    const LightShadowState* FindLightState(const void* owner);

    // Drops the claims of owners that no longer hold any tile. Call after the allocator's sweep and
    // before anything reads a claim - see TileState::Owner in Shadows.cpp for why.
    void ForgetLostClaims();

    // Drops every claim, for when shadows are switched off entirely.
    void ForgetAllClaims();
}

#pragma once

#include <vector>

#include "Pine/Rendering/Features/Shadows/ShadowAtlas/ShadowAtlas.hpp"

namespace Pine
{
    class Light;
}

// Decides which lights cast this frame, at what resolution, and what that costs them.
//
// Everything hysteretic lives here: incumbency, the challenger margin, the minimum residency, the
// promotion threshold and its lower demotion threshold, and the fade a shadow appears and
// disappears through. They are one subject because they all exist for the same reason - a tile that
// changes hands is a cold tile, and a cold tile is a full re-render - and they are only correct
// when read against each other.
//
// What is *drawn* into a granted tile, and whether that is still valid, is Shadows' business rather
// than this module's.
namespace Pine::Rendering::ShadowTileSelection
{
    // A point light needs one tile per cube face, which makes it the largest group any single light
    // can ask for. Both numbers are the same six, and deliberately so: the cost side and the
    // face-building side have to agree or a light is granted tiles it has no views for.
    constexpr int POINT_FACE_COUNT = 6;
    constexpr int MAX_LIGHT_TILE_COUNT = POINT_FACE_COUNT;

    // A light eligible for tiles this frame, scored and ranked.
    struct ShadowCandidate
    {
        Light* LightPtr = nullptr;
        float Importance = 0.f;

        // What it costs. A spot is one tile, a point light six - which is the entire difference
        // between the two as far as everything downstream is concerned.
        ShadowAtlas::TileSize Size = ShadowAtlas::TileSize::Quarter;
        int TileCount = 1;

        // Whether it held tiles coming into this frame. Incumbency is what hysteresis is about, so
        // it has to be known before any tile is handed out this frame.
        bool IsIncumbent = false;
        float ResidencyTime = 0.f;

        // How faded in its shadow was coming into this frame, 0 for a light that held nothing. A
        // candidate that loses is only worth granting tiles to while this is still above zero -
        // that is the fade-out, and it is the one case where a light that lost still draws.
        float Fade = 0.f;
    };

    // What the feature remembers about one *light's claim*, as opposed to one tile.
    //
    // Residency, fade and importance belong here rather than on a per-tile record because they are
    // properties of the claim: a point light that has held its six faces for two seconds has held
    // them for two seconds, not two seconds each, and "evict one face of six" is not a thing that
    // can happen. Cache validity is the opposite - that genuinely is per tile, because each face is
    // a different picture.
    struct LightShadowState
    {
        const void* Owner = nullptr;

        float ResidencyTime = 0.f;
        float Fade = 0.f;
        float Importance = 0.f;

        int TileCount = 0;

        // The size class its tiles currently come from. Remembered because the choice is
        // hysteretic - see SelectTileSize.
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

    // Drops the claims of owners that no longer hold any tile. Call after the allocator's sweep, and
    // before anything reads a claim: the sweep is the authority on who holds what, and a claim
    // pointing at a light that stopped casting last frame would be worse than stale, because the
    // component pool reuses slots and a new Light can be constructed at the dead one's address.
    void ForgetLostClaims();

    // Drops every claim, for when shadows are switched off entirely.
    void ForgetAllClaims();
}

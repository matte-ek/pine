#include "ShadowTileSelection.hpp"

#include <algorithm>
#include <limits>

#include "Pine/Performance/Performance.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/World/Entity/Entity.hpp"

using namespace Pine;
using namespace Rendering::ShadowTileSelection;

namespace
{
    std::vector<LightShadowState> m_LightStates;
    std::vector<ShadowCandidate> m_Candidates;

    // Importance at which a light is promoted to Large tiles, and the lower value it has to fall
    // back through before it is demoted again.
    //
    // Importance is range/distance, so 1.0 is exactly "the camera is inside the light's volume",
    // which is a good line for "this shadow is worth the resolution".
    //
    // The gap between the two is hysteresis and it is not optional: changing size means changing
    // tile, changing tile means the new one is cold, and cold means a full re-render. A light
    // hovering on a single threshold would re-render every frame - the exact cost caching exists to
    // remove, reintroduced by the thing meant to improve quality.
    constexpr float HIGH_RES_TILE_IMPORTANCE = 1.0f;
    constexpr float HIGH_RES_TILE_IMPORTANCE_DROP = 0.7f;

    // A challenger must be this much more important than an incumbent to take its tile. Without a
    // margin, two lights either side of the boundary trade the tile every frame and each trade is a
    // cold re-render of both - the exact cost caching exists to remove, paid twice over.
    constexpr float CHALLENGER_MARGIN = 1.25f;

    // ...and below this much time held, an incumbent cannot be evicted at all. A tile that is drawn
    // and then handed away a few frames later cost everything and bought nothing.
    constexpr float MIN_RESIDENCY_SECONDS = 0.5f;

    // How long a shadow takes to fade in when its light wins a tile, and out when it loses one.
    // Short enough not to read as a dissolve, long enough not to read as a pop.
    constexpr float FADE_SECONDS = 0.25f;

    // How much a light's shadow is worth to the image, as the approximate angular size of its
    // sphere of influence. Zero means nothing it touches is on screen.
    //
    // Scored against every active context rather than the primary one. Which camera "the viewer"
    // means is genuinely ambiguous with an editor viewport and a game camera both live, and in the
    // editor the primary context is the *game* one - scoring against it alone would make the
    // viewport's own lights lose tiles to lights nobody is looking at. A light important to any live
    // viewer is important.
    float ComputeImportance(const Light* light)
    {
        const auto position = light->GetParent()->GetTransform()->GetPosition();
        const float range = light->GetRange();

        float importance = 0.f;

        for (const auto* context : RenderManager::GetRenderingContexts())
        {
            if (context == nullptr || !context->Active || context->SceneCamera == nullptr)
            {
                continue;
            }

            const auto* camera = context->SceneCamera;

            const auto frustum = Frustum::FromViewProjection(
                camera->GetProjectionMatrix() * camera->GetViewMatrix());

            // The light's whole sphere of influence is off screen, so its shadow cannot be on it.
            if (!frustum.Intersects(position, range))
            {
                continue;
            }

            const auto cameraPosition = camera->GetParent()->GetTransform()->GetPosition();
            const float distance = glm::distance(position, cameraPosition);

            // range / distance is near enough the tangent of the half-angle the light subtends. It
            // passes 1 once the camera is inside the light's volume, which is the right shape: at
            // that point its shadow fills the screen and is the most important one there is.
            importance = std::max(importance, range / std::max(distance, 0.001f));
        }

        return importance;
    }

    LightShadowState* FindClaim(const void* owner)
    {
        for (auto& state : m_LightStates)
        {
            if (state.Owner == owner)
            {
                return &state;
            }
        }

        return nullptr;
    }

    LightShadowState& AcquireClaim(const void* owner)
    {
        if (auto* existing = FindClaim(owner))
        {
            return *existing;
        }

        m_LightStates.emplace_back();
        m_LightStates.back().Owner = owner;

        return m_LightStates.back();
    }

    // Ranking key. Incumbency is expressed as a handicap on the challengers rather than as a
    // separate pass, so the whole policy stays one sort: hold a tile and you are worth
    // CHALLENGER_MARGIN times what you would be worth as a newcomer; hold it for less than
    // MIN_RESIDENCY_SECONDS and you cannot be beaten at all yet.
    float SelectionScore(const ShadowCandidate& candidate)
    {
        if (!candidate.IsIncumbent)
        {
            return candidate.Importance;
        }

        if (candidate.ResidencyTime < MIN_RESIDENCY_SECONDS)
        {
            return std::numeric_limits<float>::max();
        }

        return candidate.Importance * CHALLENGER_MARGIN;
    }

    // The class importance asks for, clamped to one that can actually hold a group this size.
    //
    // The clamp is not an optimisation, it is the difference between casting and not: a group is
    // granted all-or-nothing out of a single class, so a point light's six faces cannot come from
    // the Quarter class at all - that quadrant holds four tiles. Asking anyway is not a contest the
    // light loses, it is a request that can never be granted, and it costs the light its shadow
    // outright: it holds nothing, so it has no state to be demoted from, so it asks for the same
    // impossible thing again next frame. Size following importance rather than light type is what
    // exposed this - every point light close enough to matter scores above the threshold.
    Rendering::ShadowAtlas::TileSize SelectTileSize(const float importance, const LightShadowState* state, const int tileCount)
    {
        const float threshold =
            state != nullptr && state->Size == Rendering::ShadowAtlas::TileSize::Quarter
                ? HIGH_RES_TILE_IMPORTANCE_DROP
                : HIGH_RES_TILE_IMPORTANCE;

        const auto size = importance >= threshold
            ? Rendering::ShadowAtlas::TileSize::Quarter
            : Rendering::ShadowAtlas::TileSize::Eighth;

        // Eighth is the smallest class there is, so it is the only thing to fall back to - and a
        // group that does not fit there does not fit anywhere, which Setup checks for once.
        if (Rendering::ShadowAtlas::GetTileCapacity(size) < tileCount)
        {
            return Rendering::ShadowAtlas::TileSize::Eighth;
        }

        return size;
    }
}

// The clamp in SelectTileSize is not enough on its own: it answers whether a class could ever
// hold a group this size, which is a property of the layout, and says nothing about what is free
// right now. There are four Quarter tiles, so the fifth spot light above the promotion threshold
// asks for a class that is simply full.
//
// Holding nothing is what would make that permanent. LightShadowState only exists for a light
// with tiles, so a light that failed outright has no recorded size to be demoted from next
// frame: SelectTileSize sees no state, applies the undropped threshold, picks Quarter again, and
// fails again for as long as the incumbents hold - with sixteen Eighth tiles sitting empty. A
// contest for the high-resolution class has to cost a light its resolution, not its shadow.
bool Rendering::ShadowTileSelection::AcquireTiles(const ShadowCandidate& candidate,
                                                 ShadowAtlas::TileSize* outSize, int* outSlots)
{
    if (ShadowAtlas::Acquire(candidate.Size, candidate.LightPtr, candidate.TileCount, outSlots))
    {
        *outSize = candidate.Size;

        return true;
    }

    // Eighth is the smallest class there is, so there is exactly one demotion to try and a
    // candidate already asking for it has nowhere left to go.
    if (candidate.Size == ShadowAtlas::TileSize::Eighth)
    {
        return false;
    }

    if (!ShadowAtlas::Acquire(ShadowAtlas::TileSize::Eighth, candidate.LightPtr,
                              candidate.TileCount, outSlots))
    {
        return false;
    }

    *outSize = ShadowAtlas::TileSize::Eighth;

    return true;
}

const std::vector<ShadowCandidate>& Rendering::ShadowTileSelection::SelectShadowCandidates(
const std::vector<Light*>& lights)
{
    PINE_PF_SCOPE();

    m_Candidates.clear();

    for (auto* light : lights)
    {
        auto& hintData = light->GetLightHintData();

        // Cleared for every light, casting or not: a light that loses its tile this frame must
        // not keep pointing at the view some other light now owns.
        hintData.ShadowViewIndex = -1;
        hintData.ShadowViewCount = 0;

        const auto lightType = light->GetLightType();

        if (!light->GetCastShadows() ||
            (lightType != LightType::SpotLight && lightType != LightType::PointLight))
        {
            continue;
        }

        ShadowCandidate candidate;

        candidate.LightPtr = light;
        candidate.Importance = ComputeImportance(light);

        // The only place a light type is turned into a cost. Everything downstream reads the
        // cost and never asks what kind of light produced it.
        candidate.TileCount = lightType == LightType::PointLight ? POINT_FACE_COUNT : 1;

        const auto* state = FindClaim(light);

        if (state != nullptr)
        {
            candidate.IsIncumbent = true;
            candidate.ResidencyTime = state->ResidencyTime;
            candidate.Fade = state->Fade;
        }

        // Size follows importance, not light type.
        //
        // Tying the size classes to "spot" and "point" was the first-caller shape, and it was
        // wrong on its own terms as well: the justification was that a cube face covers 90
        // degrees where a spot cone covers a narrow slice, but a spot at the default 45 degree
        // outer angle covers about 94 - the same. What a multi-size atlas is actually for is
        // giving a light that fills the screen a sharp shadow and one across the room a cheap
        // one, and that is a question about the light's importance, which is already computed.
        candidate.Size = SelectTileSize(candidate.Importance, state, candidate.TileCount);

        // Off screen and holding nothing: there is no decision to make about it. An off-screen
        // light that still holds tiles stays in the list so it can fade out rather than pop.
        if (candidate.Importance <= 0.f && !candidate.IsIncumbent)
        {
            continue;
        }

        m_Candidates.push_back(candidate);
    }

    std::sort(m_Candidates.begin(), m_Candidates.end(),
              [](const ShadowCandidate& a, const ShadowCandidate& b)
              {
                  return SelectionScore(a) > SelectionScore(b);
              });

    return m_Candidates;
}

const LightShadowState& Rendering::ShadowTileSelection::RecordGrant(const ShadowCandidate& candidate,
                                                                   const ShadowAtlas::TileSize grantedSize,
                                                                   const bool wins, const float deltaTime)
{
    const float fadeStep = FADE_SECONDS > 0.f ? deltaTime / FADE_SECONDS : 1.f;

    auto& claim = AcquireClaim(candidate.LightPtr);

    claim.ResidencyTime += deltaTime;
    claim.Importance = candidate.Importance;
    claim.Fade = std::clamp(claim.Fade + (wins ? fadeStep : -fadeStep), 0.f, 1.f);
    claim.TileCount = candidate.TileCount;

    // Recorded after the acquire, so it is what the light actually got, demotion included. A size
    // change means the incumbency scan in Acquire matched nothing (it matches on size too), so the
    // old tiles went unclaimed and the sweep frees them - the move costs one cold re-render and no
    // bookkeeping.
    claim.Size = grantedSize;

    return claim;
}

const LightShadowState* Rendering::ShadowTileSelection::FindLightState(const void* owner)
{
    return FindClaim(owner);
}

void Rendering::ShadowTileSelection::ForgetLostClaims()
{
    m_LightStates.erase(std::remove_if(m_LightStates.begin(), m_LightStates.end(),
                                       [](const LightShadowState& state)
                                       {
                                           return !ShadowAtlas::HasTiles(state.Owner);
                                       }),
                        m_LightStates.end());
}

void Rendering::ShadowTileSelection::ForgetAllClaims()
{
    m_LightStates.clear();
}

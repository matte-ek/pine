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

    // Importance at which a light is promoted to Quarter tiles, and the lower value it has to fall
    // back through before it is demoted again. 1.0 is the camera entering the light's volume.
    //
    // The gap is hysteresis: a size change moves the light to a cold tile, so a light hovering on a
    // single threshold would re-render every frame.
    constexpr float HIGH_RES_TILE_IMPORTANCE = 1.0f;
    constexpr float HIGH_RES_TILE_IMPORTANCE_DROP = 0.7f;

    // A challenger must be this much more important than an incumbent to take its tile, so two
    // lights near the boundary do not trade it every frame.
    constexpr float CHALLENGER_MARGIN = 1.25f;

    // Below this much time held, an incumbent cannot be evicted at all.
    constexpr float MIN_RESIDENCY_SECONDS = 0.5f;

    // How long a shadow takes to fade in when its light wins a tile, and out when it loses one.
    constexpr float FADE_SECONDS = 0.25f;

    // How much a light's shadow is worth to the image, as the approximate angular size of its
    // sphere of influence. Zero means nothing it touches is on screen.
    //
    // The maximum over every active context, not just the primary one: in the editor the primary
    // context is the game camera, and the viewport's lights must not lose tiles to it.
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

            // range / distance approximates the tangent of the half-angle the light subtends, and
            // passes 1 once the camera is inside the light's volume.
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

    // Ranking key. An incumbent scores CHALLENGER_MARGIN times its importance, and cannot be beaten
    // at all before MIN_RESIDENCY_SECONDS.
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

    // The class importance asks for, clamped to one that can hold a group this size at all. A
    // point light's six faces cannot come from the four-tile Quarter class, and asking for it
    // would fail every frame.
    Rendering::ShadowAtlas::TileSize SelectTileSize(const float importance, const LightShadowState* state, const int tileCount)
    {
        const float threshold =
            state != nullptr && state->Size == Rendering::ShadowAtlas::TileSize::Quarter
                ? HIGH_RES_TILE_IMPORTANCE_DROP
                : HIGH_RES_TILE_IMPORTANCE;

        const auto size = importance >= threshold
            ? Rendering::ShadowAtlas::TileSize::Quarter
            : Rendering::ShadowAtlas::TileSize::Eighth;

        // Eighth is the smallest class. Shadows::Setup checks that it fits the largest group.
        if (Rendering::ShadowAtlas::GetTileCapacity(size) < tileCount)
        {
            return Rendering::ShadowAtlas::TileSize::Eighth;
        }

        return size;
    }
}

// Falls back to Eighth when the requested class is full this frame. Without the fallback, a light
// that fails outright has no LightShadowState to be demoted through, so it would ask for the full
// class again every frame while Eighth tiles sit empty.
bool Rendering::ShadowTileSelection::AcquireTiles(const ShadowCandidate& candidate,
                                                 ShadowAtlas::TileSize* outSize, int* outSlots)
{
    if (ShadowAtlas::Acquire(candidate.Size, candidate.LightPtr, candidate.TileCount, outSlots))
    {
        *outSize = candidate.Size;

        return true;
    }

    // Already the smallest class, so there is nothing to fall back to.
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

        // Cleared for every light, casting or not. See the declaration.
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

        // The only place a light type becomes a cost; nothing downstream looks at the type.
        candidate.TileCount = lightType == LightType::PointLight ? POINT_FACE_COUNT : 1;

        const auto* state = FindClaim(light);

        if (state != nullptr)
        {
            candidate.IsIncumbent = true;
            candidate.ResidencyTime = state->ResidencyTime;
            candidate.Fade = state->Fade;
        }

        // Size follows importance, not light type.
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

    // The size actually granted, demotion included. On a size change the old tiles went unclaimed,
    // so the atlas sweep frees them.
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

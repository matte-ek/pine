#include "Shadows.hpp"

#include <algorithm>
#include <cmath>

#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Performance/Performance.hpp"
#include "Pine/Rendering/Renderer3D/Renderer3D.hpp"
#include "Pine/Rendering/Renderer3D/ShaderStorages.hpp"
#include "Pine/Rendering/Features/Shadows/ShadowView/ShadowView.hpp"
#include "Pine/Rendering/Features/Shadows/ShadowAtlas/ShadowAtlas.hpp"
#include "Pine/Rendering/Features/Shadows/ShadowCascades/ShadowCascades.hpp"
#include "Pine/Rendering/Features/Shadows/ShadowPass/ShadowPass.hpp"
#include "Pine/Rendering/Features/Shadows/ShadowTileSelection/ShadowTileSelection.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Pine/Rendering/SceneProcessor/SceneProcessor.hpp"
#include "Pine/Rendering/GraphicsSettings/GraphicsSettings.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Entity/Entity.hpp"

using namespace Pine;
using namespace Renderer3D::Specifications::Shadows;

namespace
{
    Camera* m_SceneCamera = nullptr;

    // shadowViews[0 .. CASCADE_COUNT-1] belong to the cascades, so local views start after them.
    constexpr int LOCAL_VIEW_BASE = CASCADE_COUNT;

    // Views for spot and point lights, refilled once per frame at scene level. Entry i is uploaded
    // to the shader's shadowViews[LOCAL_VIEW_BASE + i].
    //
    // Grows but never shrinks, and views are filled in place, so each view's VisibilitySet is
    // allocated once.
    std::vector<Rendering::ShadowView> m_LocalViews;
    int m_LocalViewCount = 0;

    Rendering::Shadows::Statistics m_Statistics;

    // What is cached in one atlas tile, indexed by atlas slot. Per slot rather than per light
    // because the cache is the tile's pixels, which a slot that changes hands has to forget.
    struct TileState
    {
        // Compared, never dereferenced. Resynced against the atlas sweep every frame, because the
        // component pool reuses slots: a new Light can be constructed at a dead one's address and
        // would otherwise inherit its cached tiles.
        const void* Owner = nullptr;

        // The projection the tile's contents were drawn with.
        Matrix4f ViewProjection = Matrix4f(0.f);

        bool ContentValid = false;
    };

    std::vector<TileState> m_TileStates;

    // Scratch set for the sphere pass a point light does before culling its six faces.
    Rendering::RenderCulling::VisibilitySet m_GroupVisibility;

    // How far a local light's shadow lookup pushes its sample point off the receiver, in texels of
    // that light's tile. Two, to clear the 2x2 PCF footprint. Raise it if acne appears, at the
    // cost of some contact shadow at grazing angles.
    constexpr float LOCAL_NORMAL_OFFSET_TEXELS = 2.f;

    // Whether a tile's contents have stopped being a correct picture of the scene. Walks only the
    // casters that moved, never the whole scene.
    bool IsViewStale(const Rendering::ShadowView& view,
                     const TileState& state,
                     const Rendering::SceneProcessor::SceneProcessorContext& sceneContext)
    {
        // Nothing was ever drawn here, or it was drawn for a different light.
        if (!state.ContentValid)
        {
            return true;
        }

        // The light moved or turned, or its range or cone angle changed. Compared instead of
        // Transform's dirty flag, which nothing clears for a light.
        if (state.ViewProjection != view.ViewProjection)
        {
            return true;
        }

        if (sceneContext.CasterSetChanged)
        {
            return true;
        }

        // Not narrowed to the chunks that changed: terrain edits are rare.
        if (sceneContext.TerrainChanged)
        {
            return true;
        }

        for (auto* caster : sceneContext.MovedCasters)
        {
            const auto& data = caster->GetRenderingHintData();

            // Both boxes: the view a caster left still holds its shadow.
            if (view.ViewFrustum.Intersects(data.BoundsMin, data.BoundsMax) ||
                view.ViewFrustum.Intersects(data.PreviousBoundsMin, data.PreviousBoundsMax))
            {
                return true;
            }
        }

        return false;
    }

    // A spot light's single view: a perspective frustum matching its cone. Fills 'view' in place so
    // its VisibilitySet keeps its allocation.
    void BuildSpotView(const Light* light, const int atlasSlot, Rendering::ShadowView& view)
    {
        const auto* transform = light->GetParent()->GetTransform();

        const auto position = transform->GetPosition();
        const auto forward = glm::normalize(transform->GetRotation() * Vector3f(0.f, 0.f, -1.f));

        // Any up vector not parallel to forward works; swap for a light aimed straight up or down.
        const auto up = std::abs(forward.y) > 0.999f ? Vector3f(0.f, 0.f, 1.f) : Vector3f(0.f, 1.f, 0.f);

        const float range = light->GetRange();

        // Scaled with the range: at D16 a near plane that is too close produces acne no bias can
        // absorb.
        const float nearPlane = std::max(0.05f, range * 0.005f);

        // The cone's full angle, widened so the edge PCF tap reads rendered texels.
        const float fov = glm::radians(std::min(light->GetSpotlightOuterAngle() * 2.f + 4.f, 179.f));

        const auto viewMatrix = glm::lookAt(position, position + forward, up);
        const auto projectionMatrix = glm::perspective(fov, 1.f, nearPlane, range);

        view.ViewProjection = projectionMatrix * viewMatrix;
        view.ViewFrustum = Frustum::FromViewProjection(view.ViewProjection);
        view.Origin = position;
        const auto viewport = Rendering::ShadowAtlas::GetViewport(atlasSlot);

        view.Viewport = viewport;
        view.TexelWorldScale = 2.f * std::tan(fov * 0.5f) / static_cast<float>(viewport.z);

        view.SlopeBias = Rendering::SHADOW_SEPARATION_SLOPE_BIAS;
        view.DepthBias = Rendering::SHADOW_SEPARATION_DEPTH_BIAS;

        // Written explicitly: views are refilled in place, so every field must be set by the
        // builder.
        view.FaceCulling = Graphics::FaceCullMode::Back;
    }

    // One face of a point light's cube.
    void BuildPointView(const Light* light, const int face, const int atlasSlot, Rendering::ShadowView& view)
    {
        // +X, -X, +Y, -Y, +Z, -Z: the order the shader picks a face in, by the major axis of the
        // vector from the light. The up vectors follow the GL cube map convention.
        static const Vector3f faceDirections[Rendering::ShadowTileSelection::POINT_FACE_COUNT] =
        {
            Vector3f( 1.f,  0.f,  0.f), Vector3f(-1.f,  0.f,  0.f),
            Vector3f( 0.f,  1.f,  0.f), Vector3f( 0.f, -1.f,  0.f),
            Vector3f( 0.f,  0.f,  1.f), Vector3f( 0.f,  0.f, -1.f),
        };

        static const Vector3f faceUps[Rendering::ShadowTileSelection::POINT_FACE_COUNT] =
        {
            Vector3f( 0.f, -1.f,  0.f), Vector3f( 0.f, -1.f,  0.f),
            Vector3f( 0.f,  0.f,  1.f), Vector3f( 0.f,  0.f, -1.f),
            Vector3f( 0.f, -1.f,  0.f), Vector3f( 0.f, -1.f,  0.f),
        };

        const auto position = light->GetParent()->GetTransform()->GetPosition();
        const float range = light->GetRange();

        const float nearPlane = std::max(0.05f, range * 0.005f);

        const auto viewport = Rendering::ShadowAtlas::GetViewport(atlasSlot);

        // 90 degrees, widened by a two-texel border so the edge PCF tap has something to read.
        // Together with the half-texel clamp in the shader, this hides the cube seams.
        const float halfExtent = 1.f + 2.f / static_cast<float>(viewport.z);
        const float fov = 2.f * std::atan(halfExtent);

        const auto viewMatrix = glm::lookAt(position, position + faceDirections[face], faceUps[face]);
        const auto projectionMatrix = glm::perspective(fov, 1.f, nearPlane, range);

        view.ViewProjection = projectionMatrix * viewMatrix;
        view.ViewFrustum = Frustum::FromViewProjection(view.ViewProjection);
        view.Origin = position;
        view.Viewport = viewport;
        view.TexelWorldScale = 2.f * std::tan(fov * 0.5f) / static_cast<float>(viewport.z);

        view.SlopeBias = Rendering::SHADOW_SEPARATION_SLOPE_BIAS;
        view.DepthBias = Rendering::SHADOW_SEPARATION_DEPTH_BIAS;

        // Written explicitly, as in BuildSpotView.
        view.FaceCulling = Graphics::FaceCullMode::Back;
    }

    // Fills the visible set of every stale view in one light's group. A point light's six faces
    // all fall within its sphere of influence, so one sphere pass narrows the six frustum passes.
    void BuildGroupVisibility(const Rendering::ShadowTileSelection::ShadowCandidate& candidate, const int firstViewIndex)
    {
        const Rendering::RenderCulling::VisibilitySet* restrictTo = nullptr;

        // Not worth it for a single view.
        if (candidate.TileCount > 1)
        {
            const auto position = candidate.LightPtr->GetParent()->GetTransform()->GetPosition();

            Rendering::RenderCulling::Cull(position,
                                           candidate.LightPtr->GetRange(),
                                           m_GroupVisibility,
                                           Rendering::RenderCulling::Candidates::ShadowCasters);

            restrictTo = &m_GroupVisibility;
        }

        for (int i = 0; i < candidate.TileCount; i++)
        {
            auto& view = m_LocalViews[firstViewIndex + i];

            if (!view.NeedsRender)
            {
                continue;
            }

            m_Statistics.CastersDrawn +=
                Rendering::RenderCulling::Cull(view.ViewFrustum,
                                               view.Visibility,
                                               Rendering::RenderCulling::Candidates::ShadowCasters,
                                               restrictTo).VisibleObjectCount;
        }
    }

    // Records what ShadowPass::Render just drew. Must skip on the same NeedsRender condition as
    // the pass, or a tile is marked as holding a picture that was never drawn into it.
    void RecordRenderedViews(const Rendering::ShadowView* views, const int count)
    {
        for (int i = 0; i < count; i++)
        {
            const auto& view = views[i];

            if (!view.NeedsRender || view.AtlasSlot < 0)
            {
                continue;
            }

            auto& state = m_TileStates[view.AtlasSlot];

            state.ContentValid = true;
            state.ViewProjection = view.ViewProjection;
        }
    }
}

void Rendering::Shadows::Setup()
{
    Rendering::ShadowAtlas::Setup();

    m_TileStates.resize(ShadowAtlas::GetSlots().size());

    ShadowCascades::Setup();

    // Pinned tiles are never swept, so the per-frame resync never sets their owner; seed it here.
    const auto& slots = ShadowAtlas::GetSlots();

    for (std::size_t i = 0; i < slots.size(); i++)
    {
        if (slots[i].Pinned)
        {
            m_TileStates[i].Owner = slots[i].Owner;
        }
    }

    // Otherwise such lights would silently never cast.
    if (ShadowAtlas::GetTileCapacity(ShadowAtlas::TileSize::Eighth) < ShadowTileSelection::MAX_LIGHT_TILE_COUNT)
    {
        PError(fmt::format("Shadow atlas smallest tile class holds {} tiles, fewer than the {} a single "
                           "light can ask for - lights needing a full group will never cast.",
                           ShadowAtlas::GetTileCapacity(ShadowAtlas::TileSize::Eighth),
                           ShadowTileSelection::MAX_LIGHT_TILE_COUNT));
    }

    ShadowPass::Setup();
}

void Rendering::Shadows::Shutdown()
{
    Rendering::ShadowAtlas::Shutdown();
}

void Rendering::Shadows::NewFrame(Camera* sceneCamera)
{
    m_SceneCamera = sceneCamera;
}

void Rendering::Shadows::PrepareLocalViews(const SceneProcessor::SceneProcessorContext& sceneContext)
{
    PINE_PF_SCOPE();

    m_Statistics.Reset();
    m_LocalViewCount = 0;

    ShadowAtlas::BeginFrame();

    m_TileStates.resize(ShadowAtlas::GetSlots().size());

    // Resync against last frame's sweep before anything is claimed. See TileState::Owner.
    const auto& slots = ShadowAtlas::GetSlots();

    for (std::size_t i = 0; i < m_TileStates.size(); i++)
    {
        if (slots[i].Owner == nullptr)
        {
            m_TileStates[i] = TileState();
        }
    }

    ShadowTileSelection::ForgetLostClaims();

    const auto& candidates = ShadowTileSelection::SelectShadowCandidates(sceneContext.Lights);

    const auto deltaTime = static_cast<float>(RenderManager::GetGlobalDeltaTime());

    const int budget = std::min(GraphicsSettings::GetLocalShadowTileBudget(),
                                Renderer3D::Specifications::Shadows::SHADOW_VIEW_COUNT);

    auto& shadowViewData = Renderer3D::ShaderStorages::ShadowViews.Data();

    // Spent in tiles, not in lights.
    int tilesSpent = 0;

    for (const auto& candidate : candidates)
    {
        if (LOCAL_VIEW_BASE + m_LocalViewCount + candidate.TileCount >
            Renderer3D::Specifications::Shadows::SHADOW_VIEW_COUNT)
        {
            continue;
        }

        // Candidates are sorted by score, but the budget counts tiles actually taken rather than
        // list position: an off-screen incumbent within its minimum residency sorts to the top
        // while winning nothing.
        const bool wins = candidate.Importance > 0.f && tilesSpent + candidate.TileCount <= budget;

        if (!wins)
        {
            // A loser still holding tiles keeps them until it has faded out, without counting
            // against the budget.
            if (candidate.Fade <= 0.f)
            {
                continue;
            }
        }

        int atlasSlots[ShadowTileSelection::MAX_LIGHT_TILE_COUNT];
        auto grantedSize = candidate.Size;

        if (!ShadowTileSelection::AcquireTiles(candidate, &grantedSize, atlasSlots))
        {
            continue;
        }

        if (wins)
        {
            tilesSpent += candidate.TileCount;
        }

        // After the acquire, so the claim records the size actually granted.
        const auto& lightState = ShadowTileSelection::RecordGrant(candidate, grantedSize, wins, deltaTime);

        const bool isPointLight = candidate.LightPtr->GetLightType() == LightType::PointLight;

        // Into m_LocalViews. The shader's index is offset by LOCAL_VIEW_BASE.
        const int firstLocalIndex = m_LocalViewCount;

        bool anyFaceStale = false;

        for (int face = 0; face < candidate.TileCount; face++)
        {
            const int atlasSlot = atlasSlots[face];

            auto& tileState = m_TileStates[atlasSlot];

            // The tile changed hands, so its contents belong to someone else.
            if (tileState.Owner != candidate.LightPtr)
            {
                tileState = TileState();
                tileState.Owner = candidate.LightPtr;
            }

            const int localIndex = m_LocalViewCount++;

            if (static_cast<int>(m_LocalViews.size()) <= localIndex)
            {
                m_LocalViews.emplace_back();
            }

            auto& view = m_LocalViews[localIndex];

            if (isPointLight)
            {
                BuildPointView(candidate.LightPtr, face, atlasSlot, view);
            }
            else
            {
                BuildSpotView(candidate.LightPtr, atlasSlot, view);
            }

            view.AtlasSlot = atlasSlot;
            view.NeedsRender = IsViewStale(view, tileState, sceneContext);

            anyFaceStale = anyFaceStale || view.NeedsRender;

            auto& viewData = shadowViewData.Views[LOCAL_VIEW_BASE + localIndex];

            viewData.ViewProjection = view.ViewProjection;
            viewData.TileRect = ShadowAtlas::GetUvRect(atlasSlot);

            // x = world texel size per unit distance from the light, y = normal offset in texels,
            // z = strength (the light's fade, shared by all of its faces).
            viewData.Params = Vector4f(view.TexelWorldScale, LOCAL_NORMAL_OFFSET_TEXELS, lightState.Fade, 0.f);
        }

        candidate.LightPtr->GetLightHintData().ShadowViewIndex = LOCAL_VIEW_BASE + firstLocalIndex;
        candidate.LightPtr->GetLightHintData().ShadowViewCount = candidate.TileCount;

        if (anyFaceStale)
        {
            BuildGroupVisibility(candidate, firstLocalIndex);
        }
    }

    m_Statistics.LocalViewCount = m_LocalViewCount;

    ShadowAtlas::EndFrame();

    Renderer3D::ShaderStorages::ShadowViews.Upload();
}

void Rendering::Shadows::RenderLocalViews(const ObjectBatchData& batchData)
{
    PINE_PF_SCOPE();

    int staleViews = 0;

    for (int i = 0; i < m_LocalViewCount; i++)
    {
        if (m_LocalViews[i].NeedsRender)
        {
            staleViews++;
        }
    }

    m_Statistics.TilesCached = m_LocalViewCount - staleViews;
    m_Statistics.TilesRendered = staleViews;

    // The common case in a static scene: skip the pass entirely.
    if (staleViews == 0)
    {
        return;
    }

    ShadowPass::Render(m_LocalViews.data(), m_LocalViewCount, batchData);
    RecordRenderedViews(m_LocalViews.data(), m_LocalViewCount);
}

void Rendering::Shadows::ClearLocalViews(const std::vector<Light*>& lights)
{
    for (auto* light : lights)
    {
        auto& hintData = light->GetLightHintData();

        hintData.ShadowViewIndex = -1;
        hintData.ShadowViewCount = 0;
    }

    // A frame with no claims releases every unpinned tile.
    ShadowAtlas::BeginFrame();
    ShadowAtlas::EndFrame();

    // Pinned tiles keep the state seeded at Setup.
    const auto& slots = ShadowAtlas::GetSlots();

    for (std::size_t i = 0; i < m_TileStates.size(); i++)
    {
        if (!slots[i].Pinned)
        {
            m_TileStates[i] = TileState();
        }
    }

    ShadowTileSelection::ForgetAllClaims();

    m_LocalViewCount = 0;
    m_Statistics.Reset();
}

Rendering::Shadows::TileDebugInfo Rendering::Shadows::GetTileDebugInfo(const int slot)
{
    TileDebugInfo info;

    const auto& slots = ShadowAtlas::GetSlots();

    if (slot < 0 ||
        slot >= static_cast<int>(m_TileStates.size()) ||
        slot >= static_cast<int>(slots.size()))
    {
        return info;
    }

    const auto& tileState = m_TileStates[slot];

    info.Active = tileState.Owner != nullptr;
    info.ContentValid = tileState.ContentValid;

    info.Reserved = slots[slot].Pinned;

    // Per claim, so a point light's six tiles all report the same numbers.
    if (const auto* lightState = ShadowTileSelection::FindLightState(tileState.Owner))
    {
        info.Importance = lightState->Importance;
        info.Fade = lightState->Fade;
        info.ResidencyTime = lightState->ResidencyTime;
        info.TileCount = lightState->TileCount;
    }

    return info;
}

const Rendering::Shadows::Statistics& Rendering::Shadows::GetStatistics()
{
    return m_Statistics;
}

void Rendering::Shadows::RenderPassLight(Light* light, const SceneProcessor::SceneProcessorContext& sceneContext)
{
    Graphics::GetGraphicsAPI()->SetBlendingEnabled(false);

    // Returning early is enough: PrepareLocalViews has already cleared every light's
    // ShadowViewIndex this frame.
    if (!light->GetCastShadows())
    {
        return;
    }

    if (light->GetLightType() != LightType::Directional)
    {
        return;
    }

    if (!ShadowCascades::BuildViews(light, m_SceneCamera))
    {
        return;
    }

    auto& cascadeViews = ShadowCascades::GetViews();

    for (auto& view : cascadeViews)
    {
        m_Statistics.CascadeCastersDrawn +=
            RenderCulling::Cull(view.ViewFrustum, view.Visibility, RenderCulling::Candidates::ShadowCasters).VisibleObjectCount;
        m_Statistics.CascadeViewCount++;
    }

    ShadowPass::Render(cascadeViews.data(), static_cast<int>(cascadeViews.size()), sceneContext.RenderingBatch);
    RecordRenderedViews(cascadeViews.data(), static_cast<int>(cascadeViews.size()));
}

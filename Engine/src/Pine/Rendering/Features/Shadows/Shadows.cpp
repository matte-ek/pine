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
    // A fixed reservation mirroring the tiles ShadowCascades pins: both exist for the whole process,
    // and having the two disagree would be a class of bug with no symptom until a light landed on a
    // cascade's view and inherited its matrix.
    constexpr int LOCAL_VIEW_BASE = CASCADE_COUNT;

    // Views for spot and point lights, rebuilt once per frame at scene level. Index into this
    // vector is what a light's LightHintData::ShadowViewIndex refers to, and what the shader's
    // shadowViews[] array is uploaded from.
    //
    // Grows but never shrinks, and views are filled in place rather than cleared and rebuilt: a
    // ShadowView owns a VisibilitySet, which owns a bitset sized to m_MaxObjectCount. Rebuilding the
    // vector each frame would free and reallocate every one of those every frame, and from the
    // caching step onward a view's identity has to survive frames anyway.
    std::vector<Rendering::ShadowView> m_LocalViews;
    int m_LocalViewCount = 0;

    Rendering::Shadows::Statistics m_Statistics;

    // What the shadow feature remembers about one atlas tile between frames, indexed by atlas slot.
    //
    // By slot rather than by light because the cached thing is the tile's *pixels*: a slot that
    // changes hands has to forget them, and the light that lost it has nothing left to remember.
    //
    // Kept here rather than on ShadowAtlas::Slot so the allocator stays a pure allocator. It hands
    // out rectangles and sweeps the ones nobody asked for again; what is drawn in them, and whether
    // that is still true, is not its business.
    struct TileState
    {
        // Compared, never dereferenced. The atlas sweep is the authority on ownership and this is
        // resynced against it every frame, which matters because the component pool reuses slots -
        // a new Light can be constructed at a dead one's address.
        const void* Owner = nullptr;

        // The projection the tile's contents were drawn with. The light moving, turning, or having
        // its range or cone angle edited all show up here and nowhere else.
        Matrix4f ViewProjection = Matrix4f(0.f);

        bool ContentValid = false;
    };

    std::vector<TileState> m_TileStates;

    // Scratch set for the sphere pass a point light does before culling its six faces. One is
    // enough: it is filled and consumed entirely within one light's turn.
    Rendering::RenderCulling::VisibilitySet m_GroupVisibility;

    // How far a local light's shadow lookup pushes its sample point off the receiver, in texels of
    // that light's own tile. A local view is sampled with a single hardware 2x2 tap, so the
    // footprint to clear is two texels wide, and the sin() term in the shader only ever shrinks
    // this. Raise it if acne appears; every texel costs a little contact shadow at grazing light
    // angles, which is the direction that peter-pans.
    constexpr float LOCAL_NORMAL_OFFSET_TEXELS = 2.f;

    // Whether a tile's contents have stopped being a correct picture of the scene.
    //
    // Deliberately O(movers) rather than O(scene): a cached view that has to walk every object to
    // learn it can skip its render has not saved very much.
    bool IsViewStale(const Rendering::ShadowView& view,
                     const TileState& state,
                     const Rendering::SceneProcessor::SceneProcessorContext& sceneContext)
    {
        // Nothing was ever drawn here, or it was drawn for a different light.
        if (!state.ContentValid)
        {
            return true;
        }

        // The light moved or turned, or its range or cone angle was edited - all of which land in
        // the projection and nowhere else, which is why this is compared instead of Transform's
        // dirty flag. Nothing clears that flag for a light: no pass calls OnRender on one, so it
        // would read as permanently moved and nothing would ever cache.
        if (state.ViewProjection != view.ViewProjection)
        {
            return true;
        }

        if (sceneContext.CasterSetChanged)
        {
            return true;
        }

        // Terrain casts too, and a tile drawn before the ground changed shape is a picture of the
        // old ground. Not narrowed to the chunks that moved, the way the casters below are: terrain
        // changes are rare, and a chunk is not something MovedCasters can hold.
        if (sceneContext.TerrainChanged)
        {
            return true;
        }

        for (auto* caster : sceneContext.MovedCasters)
        {
            const auto& data = caster->GetRenderingHintData();

            // Both boxes: a mover invalidates the view it *left* exactly as much as the one it
            // entered, and the view it left is the one that would otherwise keep a shadow of
            // something that is no longer there.
            if (view.ViewFrustum.Intersects(data.BoundsMin, data.BoundsMax) ||
                view.ViewFrustum.Intersects(data.PreviousBoundsMin, data.PreviousBoundsMax))
            {
                return true;
            }
        }

        return false;
    }

    // A spot light's single view: a perspective frustum matching its cone.
    //
    // Fills an existing view rather than returning a new one, so the caller's VisibilitySet keeps
    // its allocation instead of having it replaced by a fresh empty one every frame.
    void BuildSpotView(const Light* light, const int atlasSlot, Rendering::ShadowView& view)
    {
        const auto* transform = light->GetParent()->GetTransform();

        const auto position = transform->GetPosition();
        const auto forward = glm::normalize(transform->GetRotation() * Vector3f(0.f, 0.f, -1.f));

        // Any up vector works as long as it is not parallel to forward; a light aimed straight down
        // is the common case that degenerates, so swap axes for it.
        const auto up = std::abs(forward.y) > 0.999f ? Vector3f(0.f, 0.f, 1.f) : Vector3f(0.f, 1.f, 0.f);

        const float range = light->GetRange();

        // Derived from the range rather than copied from the camera's 0.01. Perspective depth
        // precision is dominated by the near/far ratio, and at D16 a near plane that is too close
        // produces acne no amount of bias can absorb.
        const float nearPlane = std::max(0.05f, range * 0.005f);

        // The cone's full angle, widened slightly so the tile carries a border: the 2x2 PCF tap at
        // the very edge of the cone would otherwise reach for texels the view never rendered.
        const float fov = glm::radians(std::min(light->GetSpotlightOuterAngle() * 2.f + 4.f, 179.f));

        const auto viewMatrix = glm::lookAt(position, position + forward, up);
        const auto projectionMatrix = glm::perspective(fov, 1.f, nearPlane, range);

        view.ViewProjection = projectionMatrix * viewMatrix;
        view.ViewFrustum = Frustum::FromViewProjection(view.ViewProjection);
        view.Origin = position;
        const auto viewport = Rendering::ShadowAtlas::GetViewport(atlasSlot);

        view.Viewport = viewport;
        view.TexelWorldScale = 2.f * std::tan(fov * 0.5f) / static_cast<float>(viewport.z);

        // Slope-scaled bias is applied by the rasterizer while rendering this view; the normal
        // offset is applied by the shader when sampling it.
        view.SlopeBias = 2.f;
        view.DepthBias = 4.f;

        // Stated rather than left at the struct default. These views are filled in place and their
        // slots are reused across frames by different lights, so every field a view is rendered with
        // has to be written by whoever builds it - and cascades, which want Front, are one merge of
        // the two view vectors away from landing in these same slots.
        view.FaceCulling = Graphics::FaceCullMode::Back;
    }

    // One face of a point light's cube.
    //
    // Six separate views rather than a layered geometry shader emitting all six: it keeps the build
    // and render loops identical to the spot and cascade paths, and with caching the six walks
    // mostly do not happen at all. Viewport-array stays in the back pocket for the one case caching
    // cannot help, which is many simultaneously *moving* point lights.
    void BuildPointView(const Light* light, const int face, const int atlasSlot, Rendering::ShadowView& view)
    {
        // Standard cube face order: +X, -X, +Y, -Y, +Z, -Z. The shader picks a face by the major
        // axis of the vector from the light in exactly this order, and then uses that face's own
        // projection - so the up vectors only have to be consistent with themselves, not with any
        // cube map convention. These are the GL ones anyway, because surprising a reader here costs
        // more than it saves.
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

        // A cube face is 90 degrees, widened by just enough to carry a two-texel border.
        //
        // This is half of what makes the seams go away, the other half being the half-texel clamp in
        // the shader. Without a border, the 2x2 PCF tap at the very edge of a face has nothing
        // beyond the edge to read, and the clamp turns into a visible line along every cube edge.
        //
        // Derived from the tile resolution rather than a fixed number of degrees, because the border
        // has to be a constant number of *texels*: half a degree is a generous border on a 128px
        // tile and not enough on a 1024px one.
        const float halfExtent = 1.f + 2.f / static_cast<float>(viewport.z);
        const float fov = 2.f * std::atan(halfExtent);

        const auto viewMatrix = glm::lookAt(position, position + faceDirections[face], faceUps[face]);
        const auto projectionMatrix = glm::perspective(fov, 1.f, nearPlane, range);

        view.ViewProjection = projectionMatrix * viewMatrix;
        view.ViewFrustum = Frustum::FromViewProjection(view.ViewProjection);
        view.Origin = position;
        view.Viewport = viewport;
        view.TexelWorldScale = 2.f * std::tan(fov * 0.5f) / static_cast<float>(viewport.z);

        view.SlopeBias = 2.f;
        view.DepthBias = 4.f;

        // See BuildSpotView: written rather than inherited from the struct default.
        view.FaceCulling = Graphics::FaceCullMode::Back;
    }

    // Fills the visible set of every stale view in one light's group.
    //
    // Culling happens here rather than at render time because this is where the group is still
    // visible *as* a group. A point light's six faces share its sphere of influence exactly, so the
    // sphere is a free superset: one cheap pass, then six frustum passes that skip almost everything
    // instead of six full-scene ones. By the time RenderLocalViews walks m_LocalViews there are only
    // views left, which is the right shape for rendering and the wrong one for this.
    void BuildGroupVisibility(const Rendering::ShadowTileSelection::ShadowCandidate& candidate, const int firstViewIndex)
    {
        const Rendering::RenderCulling::VisibilitySet* restrictTo = nullptr;

        // Not worth it for a single view - the sphere pass would cost a full scene walk to save one.
        if (candidate.TileCount > 1)
        {
            const auto position = candidate.LightPtr->GetParent()->GetTransform()->GetPosition();

            Rendering::RenderCulling::Cull(position, candidate.LightPtr->GetRange(), m_GroupVisibility);

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
                Rendering::RenderCulling::Cull(view.ViewFrustum, view.Visibility, restrictTo).VisibleObjectCount;
        }
    }

    // Records what ShadowPass::Render just drew, for the views it was given.
    //
    // The pass draws and tells the allocator; what is *in* a tile and whether that is still true is
    // this module's bookkeeping, so the write-back stays here. Walked on the same NeedsRender
    // condition the pass skips on - the two loops have to agree, or a tile is marked as holding a
    // picture that was never drawn into it.
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

    // A pinned tile is exempt from the allocator's sweep, so the per-frame resync in
    // PrepareLocalViews never clears it and its TileState has to be seeded here instead. Read back
    // from the allocator rather than from whoever reserved them: the atlas is the authority on who
    // holds what, and this module has no business knowing that the cascades are the only reserver.
    const auto& slots = ShadowAtlas::GetSlots();

    for (std::size_t i = 0; i < slots.size(); i++)
    {
        if (slots[i].Pinned)
        {
            m_TileStates[i].Owner = slots[i].Owner;
        }
    }

    // A layout that cannot serve the largest group a light can ask for is a layout in which that
    // kind of light silently never casts: Acquire refuses it, and a refusal is indistinguishable
    // from losing a contest for space. Worth failing loudly at boot rather than in a level.
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

    // Resync against the allocator before anything is claimed. Its EndFrame sweep is the authority
    // on who holds what, and bookkeeping pointing at a light that stopped casting last frame would
    // be worse than stale: the component pool reuses slots, so a new Light can be constructed at the
    // dead one's address and would silently inherit its cached tiles.
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

    // Spent in tiles, not in lights. That is what makes the budget mean the same thing for a spot
    // and for a point light, and it only became a real distinction once something cost more than one.
    int tilesSpent = 0;

    for (const auto& candidate : candidates)
    {
        if (LOCAL_VIEW_BASE + m_LocalViewCount + candidate.TileCount >
            Renderer3D::Specifications::Shadows::SHADOW_VIEW_COUNT)
        {
            continue;
        }

        // The candidates are sorted by selection score, so rank is the whole decision - but the
        // budget is spent against tiles taken, not against list position. An un-evictable incumbent that
        // has gone off screen sorts to the very top (it cannot be beaten yet) while winning nothing,
        // and counting it as a rank would let it deny tiles to a light that is genuinely on screen.
        const bool wins = candidate.Importance > 0.f && tilesSpent + candidate.TileCount <= budget;

        if (!wins)
        {
            // A loser holding nothing simply is not casting. A loser holding tiles keeps them until
            // it has faded out - a shadow that vanishes in one frame is the pop the fade exists to
            // remove. It is not counted against the budget; it is on its way out.
            if (candidate.Fade <= 0.f)
            {
                continue;
            }
        }

        int atlasSlots[ShadowTileSelection::MAX_LIGHT_TILE_COUNT];
        auto grantedSize = candidate.Size;

        // All or nothing. Four faces of six is not two-thirds of a point light shadow, it is a hard
        // discontinuity along every edge between a face that got a tile and one that did not.
        if (!ShadowTileSelection::AcquireTiles(candidate, &grantedSize, atlasSlots))
        {
            continue;
        }

        if (wins)
        {
            tilesSpent += candidate.TileCount;
        }

        // After the acquire, so the claim records the size the light actually got rather than the
        // one it asked for.
        const auto& lightState = ShadowTileSelection::RecordGrant(candidate, grantedSize, wins, deltaTime);

        const bool isPointLight = candidate.LightPtr->GetLightType() == LightType::PointLight;

        // Two indices, deliberately not the same one: firstLocalIndex is into m_LocalViews, which
        // holds only local views, while the shader's shadowViews[] has the cascades at its head.
        const int firstLocalIndex = m_LocalViewCount;

        bool anyFaceStale = false;

        for (int face = 0; face < candidate.TileCount; face++)
        {
            const int atlasSlot = atlasSlots[face];

            auto& tileState = m_TileStates[atlasSlot];

            // The tile changed hands. Whatever is drawn in it belongs to someone else.
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
            // z = strength. Strength is the light's fade, not the tile's: all six faces of a point
            // light appear and disappear together or the cube comes apart mid-transition.
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

    // The whole point of the cache: in a static scene this is the common case and the local shadow
    // pass costs nothing at all - not even the framebuffer bind and render state.
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

    // A BeginFrame with no Acquire between it and EndFrame is exactly "nobody claimed anything",
    // which is the sweep the allocator already knows how to do.
    ShadowAtlas::BeginFrame();
    ShadowAtlas::EndFrame();

    // Everything except the pinned cascade tiles, whose bookkeeping is seeded once at Setup and has
    // nothing to do with whether local lights are casting.
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

    // The two lists are the same length - m_TileStates is resized from the slot list every frame -
    // but this reads both, so it is guarded against both.
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

    // Whoever displays this cannot assume Slot::Owner is a Light - a reserved tile's owner is a
    // token belonging to whichever subsystem pinned it. Asking here is the only place that knows
    // the difference.
    info.Reserved = slots[slot].Pinned;

    // Importance, fade and residency belong to the light's whole claim, so a point light's six
    // tiles all report the same numbers - which is the point.
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

    // Honoured here as well as in ShadowTileSelection, which only ever sees spot and point
    // lights - this is the only path a directional light takes, so without this the checkbox did
    // nothing at all for the sun.
    //
    // Turning the cascades off needs nothing beyond not building them: PrepareLocalViews clears
    // every light's ShadowViewIndex once per frame, before any context reaches its prepass, so a
    // light that never reaches ShadowCascades::BuildViews keeps the -1 that tells the shader it has
    // no views.
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
            RenderCulling::Cull(view.ViewFrustum, view.Visibility).VisibleObjectCount;
        m_Statistics.CascadeViewCount++;
    }

    ShadowPass::Render(cascadeViews.data(), static_cast<int>(cascadeViews.size()), sceneContext.RenderingBatch);
    RecordRenderedViews(cascadeViews.data(), static_cast<int>(cascadeViews.size()));
}

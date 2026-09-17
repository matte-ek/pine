#include "Shadows.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Performance/Performance.hpp"
#include "Pine/Rendering/Renderer3D/Renderer3D.hpp"
#include "Pine/Rendering/Renderer3D/ShaderStorages.hpp"
#include "Pine/Rendering/Pipeline/Pipeline3D/Pipeline3D.hpp"
#include "Pine/Rendering/ShadowView/ShadowView.hpp"
#include "Pine/Rendering/Features/Shadows/ShadowAtlas/ShadowAtlas.hpp"
#include "Pine/Rendering/Features/TerrainRenderer/TerrainRenderer.hpp"
#include "Pine/Engine/Engine.hpp"
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
    Shader* m_ShadowShader = nullptr;

    // The cascades' pinned atlas tiles, reserved once at Setup.
    //
    // The owner is a token rather than a Light: the allocator only ever compares owner pointers, and
    // there is nothing stable to point at here - a level can swap its sun, or have none, without the
    // reservation changing hands. Everything else in the atlas is owned by a Light, which is why
    // anything reading Slot::Owner has to ask before assuming.
    // What terrain is drawn with in every shadow view, cascade or local. See the draw itself in
    // RenderViews: a height field cannot use the cascades' front-face culling, so it pays for its
    // separation the way a local light does.
    constexpr float TERRAIN_SLOPE_BIAS = 2.f;
    constexpr float TERRAIN_DEPTH_BIAS = 4.f;

    const char m_CascadeOwner = 0;
    int m_CascadeSlots[CASCADE_COUNT] = {};
    bool m_CascadeSlotsReserved = false;

    // shadowViews[0 .. CASCADE_COUNT-1] belong to the cascades, so local views start after them.
    // A fixed reservation mirroring the pinned tiles: both exist for the whole process, and having
    // the two disagree would be a class of bug with no symptom until a light landed on a cascade's
    // view and inherited its matrix.
    constexpr int LOCAL_VIEW_BASE = CASCADE_COUNT;

    // One view per cascade, kept across frames rather than rebuilt: a VisibilitySet owns a bitset
    // sized to m_MaxObjectCount, and from step 4 onward a view's identity has to survive frames for
    // caching to mean anything.
    std::vector<Rendering::ShadowView> m_CascadeViews;

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

    // What the feature remembers about one *light's claim*, as opposed to one tile.
    //
    // Residency, fade and importance belong here rather than on TileState because they are
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
        Rendering::ShadowAtlas::TileSize Size = Rendering::ShadowAtlas::TileSize::Eighth;
    };

    std::vector<LightShadowState> m_LightStates;

    // A light eligible for tiles this frame, scored and ranked.
    struct ShadowCandidate
    {
        Light* LightPtr = nullptr;
        float Importance = 0.f;

        // What it costs. A spot is one tile, a point light six - which is the entire difference
        // between the two as far as everything below here is concerned.
        Rendering::ShadowAtlas::TileSize Size = Rendering::ShadowAtlas::TileSize::Quarter;
        int TileCount = 1;

        // Whether it held tiles coming into this frame. Incumbency is what hysteresis is about, so
        // it has to be known before any tile is handed out this frame.
        bool IsIncumbent = false;
        float ResidencyTime = 0.f;
    };

    std::vector<ShadowCandidate> m_Candidates;

    // Scratch set for the sphere pass a point light does before culling its six faces. One is
    // enough: it is filled and consumed entirely within one light's turn.
    Rendering::RenderCulling::VisibilitySet m_GroupVisibility;

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

    constexpr int POINT_FACE_COUNT = 6;

    // How far a local light's shadow lookup pushes its sample point off the receiver, in texels of
    // that light's own tile. A local view is sampled with a single hardware 2x2 tap, so the
    // footprint to clear is two texels wide, and the sin() term in the shader only ever shrinks
    // this. Raise it if acne appears; every texel costs a little contact shadow at grazing light
    // angles, which is the direction that peter-pans.
    constexpr float LOCAL_NORMAL_OFFSET_TEXELS = 2.f;

    // The largest group any single light can ask for, which is the cube.
    constexpr int MAX_LIGHT_TILE_COUNT = POINT_FACE_COUNT;

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

    Vector3f ComputeBoxCenter(const std::array<Vector3f, 8>& corners)
    {
        auto min = Vector3f(std::numeric_limits<float>::max());
        auto max = Vector3f(std::numeric_limits<float>::lowest());

        for (const auto& corner : corners)
        {
            min = glm::min(min, corner);
            max = glm::max(max, corner);
        }

        return (min + max) / 2.f;
    }

    Matrix4f BuildViewMatrix(const std::array<Vector3f, 8>& corners, const Vector3f lightDirection)
    {
        const auto center = ComputeBoxCenter(corners);

        return glm::lookAt(center - lightDirection, center, Vector3f(0.f, 1.f, 0.f));
    }

    // How far the cascade's near plane is pushed toward the light, in light space.
    //
    // An orthographic cascade is not a camera: a caster sitting *behind* the box on the light side
    // still casts into it. Clipping it away at the box's own front face is what makes a shadow wink
    // out when the camera turns, so the near plane is fitted to the geometry that can actually reach
    // the box instead.
    //
    // Only casters whose light-space XY overlaps the box can contribute, and that test is what keeps
    // one tall object on the far side of the level from stretching every cascade's depth range.
    // Ortho depth is linear, so a range that is somewhat too generous costs precision in proportion
    // rather than falling off a cliff the way a perspective near plane does.
    // How far towards the light one caster's box reaches, or nothing when it sits outside the
    // column of light above the cascade box - whatever that one casts lands somewhere else.
    std::optional<float> FitBoxNearZ(const Matrix4f& viewMatrix,
                                     const Vector3f& boundsMin, const Vector3f& boundsMax,
                                     const float minX, const float maxX,
                                     const float minY, const float maxY)
    {
        auto lightMin = Vector3f(std::numeric_limits<float>::max());
        auto lightMax = Vector3f(std::numeric_limits<float>::lowest());

        for (int corner = 0; corner < 8; corner++)
        {
            const auto worldCorner = Vector3f(
                corner & 1 ? boundsMax.x : boundsMin.x,
                corner & 2 ? boundsMax.y : boundsMin.y,
                corner & 4 ? boundsMax.z : boundsMin.z);

            const auto lightCorner = Vector3f(viewMatrix * Vector4f(worldCorner, 1.f));

            lightMin = glm::min(lightMin, lightCorner);
            lightMax = glm::max(lightMax, lightCorner);
        }

        if (lightMax.x < minX || lightMin.x > maxX ||
            lightMax.y < minY || lightMin.y > maxY)
        {
            return std::nullopt;
        }

        return lightMax.z;
    }

    float FitCasterNearZ(const Matrix4f& viewMatrix,
                         const float minX, const float maxX,
                         const float minY, const float maxY,
                         const float boxMaxZ)
    {
        PINE_PF_SCOPE();

        // The light view looks down -Z, so larger z is *closer to the light*. The box's own front
        // face is the floor: fitting can only ever push the plane further back, never crop the box.
        float nearZ = boxMaxZ;

        for (auto& modelRenderer : Components::Get<ModelRenderer>())
        {
            if (!modelRenderer.GetModel())
            {
                continue;
            }

            const auto& data = modelRenderer.GetRenderingHintData();

            if (const auto casterZ = FitBoxNearZ(viewMatrix, data.BoundsMin, data.BoundsMax, minX, maxX, minY, maxY))
            {
                nearZ = std::max(nearZ, casterZ.value());
            }
        }

        // Terrain casts as well, and is usually the tallest thing in the level. A ridge the cascade
        // box does not contain still throws a shadow across the ground that it does, so leaving the
        // chunks out here would crop exactly the shadow terrain exists to produce.
        for (const auto& terrainRenderer : Components::Get<TerrainRendererComponent>())
        {
            const auto terrain = terrainRenderer.GetTerrain();

            if (terrain == nullptr)
            {
                continue;
            }

            // Chunk bounds are terrain-local, and the terrain sits wherever its entity does.
            const auto entityPosition = terrainRenderer.GetParent()->GetTransform()->GetPosition();

            for (const auto& chunk : terrain->GetChunks())
            {
                const auto chunkZ = FitBoxNearZ(viewMatrix,
                                                chunk.BoundsMin + entityPosition,
                                                chunk.BoundsMax + entityPosition,
                                                minX, maxX, minY, maxY);

                if (chunkZ)
                {
                    nearZ = std::max(nearZ, chunkZ.value());
                }
            }
        }

        return nearZ;
    }

    Matrix4f BuildProjectionMatrix(const std::array<Vector3f, 8>& corners, const Matrix4f &viewMatrix, const float farPlaneMargin, const int shadowMapResolution)
    {
        // Use the frustum's bounding sphere for the X/Y extents. Its radius is
        // independent of camera orientation, so the ortho box stays a constant
        // size as the camera turns, which (together with the texel snap below)
        // stops the shadow edges from crawling/shimmering as the camera moves.
        const auto center = ComputeBoxCenter(corners);

        float radius = 0.f;
        for (const auto& corner : corners)
        {
            radius = std::max(radius, glm::distance(center, corner));
        }

        const auto centerLightSpace = Vector3f(viewMatrix * Vector4f(center, 1.f));

        float minX = centerLightSpace.x - radius;
        float minY = centerLightSpace.y - radius;

        // Snap the box origin to whole-texel increments in light space. The light
        // view orientation is fixed frame-to-frame, so this keeps each texel
        // mapping to a stable world region.
        const float texelSize = (2.f * radius) / static_cast<float>(shadowMapResolution);
        minX = std::floor(minX / texelSize) * texelSize;
        minY = std::floor(minY / texelSize) * texelSize;

        const float maxX = minX + 2.f * radius;
        const float maxY = minY + 2.f * radius;

        // Keep a tight depth range from the actual corners so precision isn't wasted.
        float minZ = std::numeric_limits<float>::max();
        float maxZ = std::numeric_limits<float>::lowest();

        for (const auto& corner : corners)
        {
            const float z = (viewMatrix * Vector4f(corner, 1.f)).z;
            minZ = std::min(minZ, z);
            maxZ = std::max(maxZ, z);
        }

        // glm::ortho takes zNear/zFar as positive distances *along* the view direction, while light
        // space puts everything the view can see at negative z. The two used to be passed straight
        // through, and that only produced a usable box because the light eye sits exactly one unit
        // from the box centre and farPlaneMargin was always >= 2. The accident handed the near plane
        // (2 + farPlaneMargin) of slack, which is the only reason casters behind the box cast at all
        // today - and why cascade 1 got roughly three times as much slack as cascade 0 for no reason
        // anyone chose. Both ends are explicit now.
        //
        // A hair of extra room on the near plane so a caster sitting exactly on the fitted plane is
        // not clipped by it.
        constexpr float casterNearMargin = 0.5f;

        const float nearZ = FitCasterNearZ(viewMatrix, minX, maxX, minY, maxY, maxZ) + casterNearMargin;

        return glm::ortho(minX, maxX, minY, maxY, -nearZ, -minZ + farPlaneMargin);
    }

    // Every shadow view, cascade or local, is culled by the same plain frustum test - see
    // FitCasterNearZ for why that is now true of the cascades as well. A spot or point face never
    // needed anything else: the light sits at the apex, so nothing can be between it and the near
    // plane and still cast into the view.
    //
    // Cascades previously used a whole-scene distance test instead, which is gone. The cascade far
    // plane already derives from MAX_SHADOW_DISTANCE, so the box *is* the shadow distance expressed
    // as a volume - keeping a radius test on top of it culled by transform position rather than
    // bounds and cut shadows off inside the box it was meant to approximate.

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

    LightShadowState* FindLightState(const void* owner)
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

    LightShadowState& AcquireLightState(const void* owner)
    {
        if (auto* existing = FindLightState(owner))
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

    // Claims a candidate's group of tiles, demoting it to the smallest class if the class it asked
    // for has no room left this frame. Writes the size actually granted to 'outSize', and returns
    // false only when nothing could be granted at all.
    //
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
    bool AcquireTiles(const ShadowCandidate& candidate, Rendering::ShadowAtlas::TileSize* outSize, int* outSlots)
    {
        if (Rendering::ShadowAtlas::Acquire(candidate.Size, candidate.LightPtr, candidate.TileCount, outSlots))
        {
            *outSize = candidate.Size;

            return true;
        }

        // Eighth is the smallest class there is, so there is exactly one demotion to try and a
        // candidate already asking for it has nowhere left to go.
        if (candidate.Size == Rendering::ShadowAtlas::TileSize::Eighth)
        {
            return false;
        }

        if (!Rendering::ShadowAtlas::Acquire(Rendering::ShadowAtlas::TileSize::Eighth, candidate.LightPtr,
                                             candidate.TileCount, outSlots))
        {
            return false;
        }

        *outSize = Rendering::ShadowAtlas::TileSize::Eighth;

        return true;
    }

    void SelectShadowCandidates(const std::vector<Light*>& lights)
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

            const auto* state = FindLightState(light);

            if (state != nullptr)
            {
                candidate.IsIncumbent = true;
                candidate.ResidencyTime = state->ResidencyTime;
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
    }

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
        static const Vector3f faceDirections[POINT_FACE_COUNT] =
        {
            Vector3f( 1.f,  0.f,  0.f), Vector3f(-1.f,  0.f,  0.f),
            Vector3f( 0.f,  1.f,  0.f), Vector3f( 0.f, -1.f,  0.f),
            Vector3f( 0.f,  0.f,  1.f), Vector3f( 0.f,  0.f, -1.f),
        };

        static const Vector3f faceUps[POINT_FACE_COUNT] =
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
    void BuildGroupVisibility(const ShadowCandidate& candidate, const int firstViewIndex)
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

    // Builds one ShadowView per cascade, into the atlas tiles pinned at Setup.
    //
    // Writes shadowViews[0 .. CASCADE_COUNT-1] and uploads. This runs per rendering context, unlike
    // the local views, because a cascade is derived from the camera frustum - an editor viewport and
    // a game camera genuinely need different ones, and each renders immediately before it draws.
    void BuildCascadeViews(Light* light, const Vector3f lightDirection)
    {
        m_CascadeViews.resize(CASCADE_COUNT);

        const float oldNearPane = m_SceneCamera->GetNearPlane();
        const float oldFarPlane = m_SceneCamera->GetFarPlane();

        const std::array<float, CASCADE_COUNT> farPlane = { 10.f, sqrtf(MAX_SHADOW_DISTANCE) + 5.f };

        auto& shadowViewData = Renderer3D::ShaderStorages::ShadowViews.Data();

        for (int i = 0; i < CASCADE_COUNT; i++)
        {
            if (i != 0)
            {
                m_SceneCamera->SetNearPlane(oldNearPane + farPlane[i - 1]);
            }

            m_SceneCamera->SetFarPlane(farPlane[i]);
            m_SceneCamera->OnRender(0.f);

            const auto atlasSlot = m_CascadeSlots[i];
            const auto viewport = Rendering::ShadowAtlas::GetViewport(atlasSlot);

            const auto frustumCorners = m_SceneCamera->GetFrustumCorners();

            const auto viewMatrix = BuildViewMatrix(frustumCorners, lightDirection);

            // The texel snap has to be against the tile the cascade actually lands in, not against a
            // separate resolution setting. It used to be GraphicsSettings::ShadowMapResolution, which
            // described a texture that no longer exists.
            const auto projectionMatrix = BuildProjectionMatrix(frustumCorners, viewMatrix, farPlane[i] * 0.5f, viewport.z);

            const auto viewProjection = projectionMatrix * viewMatrix;

            auto& view = m_CascadeViews[i];

            view.ViewProjection = viewProjection;
            view.ViewFrustum = Frustum::FromViewProjection(viewProjection);
            view.Origin = m_SceneCamera->GetParent()->GetTransform()->GetPosition();
            view.Viewport = viewport;
            view.AtlasSlot = atlasSlot;

            // Front-face culling instead of a bias pair - see ShadowView::FaceCulling.
            view.FaceCulling = Graphics::FaceCullMode::Front;
            view.SlopeBias = 0.f;
            view.DepthBias = 0.f;

            // Always. A cascade's projection follows the camera's depth range, which changes on any
            // camera movement at all even when the texel snap holds its XY steady - so the cache
            // would essentially never hit, and a cascade that wrongly believed itself valid is the
            // most visible stale tile there is. Caching these wants its own signal, not this one.
            view.NeedsRender = true;

            auto& viewData = shadowViewData.Views[i];

            viewData.ViewProjection = viewProjection;
            viewData.TileRect = Rendering::ShadowAtlas::GetUvRect(atlasSlot);

            // No normal offset: front-face culling already provides the separation it exists to
            // buy, and stacking them would peter-pan. The texel scale in x is perspective-only and
            // goes unread here for the same reason. Strength is 1 - a cascade never fades because
            // it never competes for its tile.
            viewData.Params = Vector4f(0.f, 0.f, 1.f, 0.f);
        }

        // The directional light points at the head of the view array, and spans it. The shader then
        // picks a cascade the same way it picks a cube face: from the count, without knowing which
        // kind of light it is looking at.
        light->GetLightHintData().ShadowViewIndex = 0;
        light->GetLightHintData().ShadowViewCount = CASCADE_COUNT;

        Renderer3D::ShaderStorages::ShadowViews.Upload();

        m_SceneCamera->SetNearPlane(oldNearPane);
        m_SceneCamera->SetFarPlane(oldFarPlane);
        m_SceneCamera->OnRender(0.f);
    }

    // Renders a run of shadow views into the shadow atlas.
    //
    // The one place shadow depth is drawn, for cascades and local lights alike. Everything that used
    // to differ between the two is carried by the view now - its tile, its face culling, its bias
    // pair - so this loop has no idea which kind it is looking at. That deletion is what folding the
    // cascades into the atlas actually bought; the memory saving was a side effect.
    void RenderViews(Rendering::ShadowView* views, const int count, const Rendering::ObjectBatchData& batchData)
    {
        PINE_PF_SCOPE();

        auto* graphicsApi = Graphics::GetGraphicsAPI();
        auto& renderSettings = Renderer3D::GetRenderConfiguration();

        Rendering::ShadowAtlas::GetFrameBuffer()->Bind();

        graphicsApi->SetDepthTestEnabled(true);
        graphicsApi->SetDepthBiasEnabled(true);

        Renderer3D::FrameReset();
        Renderer3D::UseRenderingContext(nullptr);

        renderSettings.OverrideShader = m_ShadowShader;
        renderSettings.IgnoreShaderVersions = true;
        renderSettings.SkipMaterialInitialization = true;

        // Scissor, not just viewport: glViewport does not restrict glClear, so without this the only
        // way to clear one tile would be to clear the whole atlas - which would wipe every other
        // tile, and those tiles are exactly what the cache is keeping.
        graphicsApi->SetScissorEnabled(true);

        for (int i = 0; i < count; i++)
        {
            auto& view = views[i];

            if (!view.NeedsRender)
            {
                continue;
            }

            const auto viewport = Vector2i(view.Viewport.x, view.Viewport.y);
            const auto size = Vector2i(view.Viewport.z, view.Viewport.w);

            graphicsApi->SetViewport(viewport, size);
            graphicsApi->SetScissor(viewport, size);
            graphicsApi->ClearBuffers(Graphics::DepthBuffer);

            graphicsApi->SetFaceCullingMode(view.FaceCulling);
            graphicsApi->SetDepthBias(view.SlopeBias, view.DepthBias);

            Renderer3D::SetViewProjection(view.ViewProjection);

            // Both modes, matching what the old whole-batch draw did: it ignored the material
            // rendering mode entirely, so alpha-tested geometry cast a solid shadow. RenderBatch
            // filters by mode, so leaving out the Discard pass would silently stop foliage casting.
            Pipeline3D::RenderBatch(batchData.OpaqueObjects, MaterialRenderingMode::Opaque, view.Visibility);
            Pipeline3D::RenderBatch(batchData.OpaqueObjects, MaterialRenderingMode::Discard, view.Visibility);

            // Terrain is not in the batch, so it needs its own call or the ground casts nothing.
            // It culls its own chunks against this view rather than reading view.Visibility, which
            // cannot hold them - see RenderCulling::VisibilitySet. No statistics sink: the atlas is
            // drawn once for the whole scene and belongs to no rendering context.
            //
            // Drawn with its own culling and bias rather than the view's, because a height field is
            // single-sided: it has one surface per column and no far side at all. A cascade culls
            // front faces to buy its separation for free, which for terrain would discard the
            // ground itself and leave only the chunk skirts writing depth. Back faces culled and an
            // explicit bias pair instead - the same trade a local view already makes, and for the
            // same reason its comment gives.
            graphicsApi->SetFaceCullingMode(Graphics::FaceCullMode::Back);
            graphicsApi->SetDepthBias(TERRAIN_SLOPE_BIAS, TERRAIN_DEPTH_BIAS);

            // Without skirts: they hang below the surface to hide a crack between detail levels,
            // and a vertical rim at every chunk edge writing depth casts a wall's shadow across the
            // ground next to it. See TerrainView::DrawSkirts.
            Rendering::TerrainRenderer::Render({ view.ViewFrustum, view.Origin, nullptr, false });

            graphicsApi->SetFaceCullingMode(view.FaceCulling);
            graphicsApi->SetDepthBias(view.SlopeBias, view.DepthBias);

            if (view.AtlasSlot >= 0)
            {
                // Recorded where the render happened rather than where the tile was reserved.
                auto& state = m_TileStates[view.AtlasSlot];

                state.ContentValid = true;
                state.ViewProjection = view.ViewProjection;

                Rendering::ShadowAtlas::MarkRendered(view.AtlasSlot);
            }
        }

        graphicsApi->SetScissorEnabled(false);
        graphicsApi->SetDepthBiasEnabled(false);
        graphicsApi->SetDepthBias(0.f, 0.f);
        graphicsApi->SetFaceCullingMode(Graphics::FaceCullMode::Back);

        renderSettings.OverrideShader = nullptr;
        renderSettings.IgnoreShaderVersions = false;
        renderSettings.SkipMaterialInitialization = false;
    }

    void HandleDirectionalShadowMap(Light* light, const Rendering::SceneProcessor::SceneProcessorContext& sceneContext)
    {
        if (m_SceneCamera == nullptr)
        {
            return;
        }

        const auto direction = light->GetParent()->GetTransform()->GetRotation() * Vector3f(0.f, 0.f, -1.f);

        BuildCascadeViews(light, direction);

        for (auto& view : m_CascadeViews)
        {
            m_Statistics.CascadeCastersDrawn +=
                Rendering::RenderCulling::Cull(view.ViewFrustum, view.Visibility).VisibleObjectCount;
            m_Statistics.CascadeViewCount++;
        }

        RenderViews(m_CascadeViews.data(), CASCADE_COUNT, sceneContext.RenderingBatch);
    }
}

void Rendering::Shadows::Setup()
{
    Rendering::ShadowAtlas::Setup();

    m_TileStates.resize(ShadowAtlas::GetSlots().size());

    // Pinned for the process lifetime, before any light can compete for them. The cascades are
    // structural rather than contended: they exist whenever the level has a sun, and a frame in
    // which they lost a contest for space would just be a frame with no sun shadows.
    m_CascadeSlotsReserved = ShadowAtlas::Reserve(ShadowAtlas::TileSize::Half, &m_CascadeOwner,
                                                  CASCADE_COUNT, m_CascadeSlots);

    if (!m_CascadeSlotsReserved)
    {
        PError(fmt::format("Shadow atlas has no room for {} cascade tiles - directional shadows are off. "
                           "The atlas layout has to provide at least CASCADE_COUNT tiles of the largest class.",
                           CASCADE_COUNT));
    }
    else
    {
        // The cascade tiles are pinned, so the per-frame resync in PrepareLocalViews never clears
        // them and their TileState has to be seeded here instead.
        for (const int slot : m_CascadeSlots)
        {
            m_TileStates[slot].Owner = &m_CascadeOwner;
        }
    }

    // A layout that cannot serve the largest group a light can ask for is a layout in which that
    // kind of light silently never casts: Acquire refuses it, and a refusal is indistinguishable
    // from losing a contest for space. Worth failing loudly at boot rather than in a level.
    if (ShadowAtlas::GetTileCapacity(ShadowAtlas::TileSize::Eighth) < MAX_LIGHT_TILE_COUNT)
    {
        PError(fmt::format("Shadow atlas smallest tile class holds {} tiles, fewer than the {} a single "
                           "light can ask for - lights needing a full group will never cast.",
                           ShadowAtlas::GetTileCapacity(ShadowAtlas::TileSize::Eighth), MAX_LIGHT_TILE_COUNT));
    }

    m_ShadowShader = Assets::Get<Shader>("engine/shaders/3d/shadow");
    assert(m_ShadowShader != nullptr);
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

    m_LightStates.erase(std::remove_if(m_LightStates.begin(), m_LightStates.end(),
                                       [](const LightShadowState& state)
                                       {
                                           return !ShadowAtlas::HasTiles(state.Owner);
                                       }),
                        m_LightStates.end());

    SelectShadowCandidates(sceneContext.Lights);

    const auto deltaTime = static_cast<float>(RenderManager::GetGlobalDeltaTime());
    const float fadeStep = FADE_SECONDS > 0.f ? deltaTime / FADE_SECONDS : 1.f;

    const int budget = std::min(GraphicsSettings::GetLocalShadowTileBudget(),
                                Renderer3D::Specifications::Shadows::SHADOW_VIEW_COUNT);

    auto& shadowViewData = Renderer3D::ShaderStorages::ShadowViews.Data();

    // Spent in tiles, not in lights. That is what makes the budget mean the same thing for a spot
    // and for a point light, and it only became a real distinction once something cost more than one.
    int tilesSpent = 0;

    for (const auto& candidate : m_Candidates)
    {
        if (LOCAL_VIEW_BASE + m_LocalViewCount + candidate.TileCount >
            Renderer3D::Specifications::Shadows::SHADOW_VIEW_COUNT)
        {
            continue;
        }

        // m_Candidates is sorted by SelectionScore, so rank is the whole decision - but the budget
        // is spent against tiles taken, not against list position. An un-evictable incumbent that
        // has gone off screen sorts to the very top (it cannot be beaten yet) while winning nothing,
        // and counting it as a rank would let it deny tiles to a light that is genuinely on screen.
        const bool wins = candidate.Importance > 0.f && tilesSpent + candidate.TileCount <= budget;

        const auto* incumbentState = FindLightState(candidate.LightPtr);

        if (!wins)
        {
            // A loser holding nothing simply is not casting. A loser holding tiles keeps them until
            // it has faded out - a shadow that vanishes in one frame is the pop the fade exists to
            // remove. It is not counted against the budget; it is on its way out.
            if (incumbentState == nullptr || incumbentState->Fade <= 0.f)
            {
                continue;
            }
        }

        int atlasSlots[MAX_LIGHT_TILE_COUNT];
        auto grantedSize = candidate.Size;

        // All or nothing. Four faces of six is not two-thirds of a point light shadow, it is a hard
        // discontinuity along every edge between a face that got a tile and one that did not.
        if (!AcquireTiles(candidate, &grantedSize, atlasSlots))
        {
            continue;
        }

        if (wins)
        {
            tilesSpent += candidate.TileCount;
        }

        auto& lightState = AcquireLightState(candidate.LightPtr);

        lightState.ResidencyTime += deltaTime;
        lightState.Importance = candidate.Importance;
        lightState.Fade = std::clamp(lightState.Fade + (wins ? fadeStep : -fadeStep), 0.f, 1.f);
        lightState.TileCount = candidate.TileCount;

        // Recorded after the acquire, so it is what the light actually got, demotion included. A
        // size change means the incumbency scan in Acquire matched nothing (it matches on size too),
        // so the old tiles went unclaimed and the sweep frees them - the move costs one cold
        // re-render and no bookkeeping.
        lightState.Size = grantedSize;

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

    RenderViews(m_LocalViews.data(), m_LocalViewCount, batchData);
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

    m_LightStates.clear();

    m_LocalViewCount = 0;
    m_Statistics.Reset();
}

Rendering::Shadows::TileDebugInfo Rendering::Shadows::GetTileDebugInfo(const int slot)
{
    TileDebugInfo info;

    if (slot < 0 || slot >= static_cast<int>(m_TileStates.size()))
    {
        return info;
    }

    const auto& tileState = m_TileStates[slot];

    info.Active = tileState.Owner != nullptr;
    info.ContentValid = tileState.ContentValid;

    // Whoever displays this cannot assume Slot::Owner is a Light - the cascades' tiles are owned by
    // a token. Asking here is the only place that knows the difference.
    info.Reserved = tileState.Owner == &m_CascadeOwner;

    // Importance, fade and residency belong to the light's whole claim, so a point light's six
    // tiles all report the same numbers - which is the point.
    if (const auto* lightState = FindLightState(tileState.Owner))
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

    // Honoured here as well as in SelectShadowCandidates, which only ever sees spot and point
    // lights - this is the only path a directional light takes, so without this the checkbox did
    // nothing at all for the sun.
    //
    // Turning the cascades off needs nothing beyond not building them: PrepareLocalViews clears
    // every light's ShadowViewIndex once per frame, before any context reaches its prepass, so a
    // light that never gets to BuildCascadeViews keeps the -1 that tells the shader it has no views.
    if (!light->GetCastShadows())
    {
        return;
    }

    if (light->GetLightType() == LightType::Directional && m_CascadeSlotsReserved)
    {
        HandleDirectionalShadowMap(light, sceneContext);
    }
}

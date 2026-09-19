#include "ShadowCascades.hpp"

#include <array>
#include <cmath>
#include <optional>

#include "Pine/Core/Log/Log.hpp"
#include "Pine/Rendering/Features/Shadows/ShadowAtlas/ShadowAtlas.hpp"
#include "Pine/Rendering/Renderer3D/Renderer3D.hpp"
#include "Pine/Rendering/Renderer3D/ShaderStorages.hpp"
#include "Pine/Performance/Performance.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Components/TerrainRenderer/TerrainRendererComponent.hpp"
#include "Pine/World/Entity/Entity.hpp"

using namespace Pine;
using namespace Renderer3D::Specifications::Shadows;

namespace
{
    // The cascades' pinned atlas tiles, reserved once at Setup.
    //
    // The owner is a token rather than a Light: the allocator only ever compares owner pointers, and
    // there is nothing stable to point at here - a level can swap its sun, or have none, without the
    // reservation changing hands. Everything else in the atlas is owned by a Light, which is why
    // anything reading Slot::Owner has to ask before assuming.
    const char m_TileOwner = 0;
    int m_Slots[CASCADE_COUNT] = {};
    bool m_SlotsReserved = false;

    // One view per cascade, kept across frames rather than rebuilt: a VisibilitySet owns a bitset
    // sized to m_MaxObjectCount, and a view's identity has to survive frames for culling not to pay
    // for that allocation over and over.
    std::vector<Rendering::ShadowView> m_Views;

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
        // The light view is affine, so its bounds follow from the box's centre and extents rather
        // than from its eight corners - see Math::TransformBounds. This runs for every caster in
        // the scene, per cascade, so the eight matrix-vector products it replaces were the bulk of
        // what building a cascade cost.
        Vector3f lightMin;
        Vector3f lightMax;

        Math::TransformBounds(Matrix3f(viewMatrix), Vector3f(viewMatrix[3]),
                              boundsMin, boundsMax, lightMin, lightMax);

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
}

void Rendering::ShadowCascades::Setup()
{
    // Pinned for the process lifetime, before any light can compete for them. The cascades are
    // structural rather than contended: they exist whenever the level has a sun, and a frame in
    // which they lost a contest for space would just be a frame with no sun shadows.
    m_SlotsReserved = ShadowAtlas::Reserve(ShadowAtlas::TileSize::Half, &m_TileOwner,
                                           CASCADE_COUNT, m_Slots);

    if (!m_SlotsReserved)
    {
        PError(fmt::format("Shadow atlas has no room for {} cascade tiles - directional shadows are off. "
                           "The atlas layout has to provide at least CASCADE_COUNT tiles of the largest class.",
                           CASCADE_COUNT));
    }
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
bool Rendering::ShadowCascades::BuildViews(Light* light, Camera* sceneCamera)
{
    PINE_PF_SCOPE();

    if (sceneCamera == nullptr || !m_SlotsReserved)
    {
        return false;
    }

    const auto lightDirection = light->GetParent()->GetTransform()->GetRotation() * Vector3f(0.f, 0.f, -1.f);

    m_Views.resize(CASCADE_COUNT);

    const float oldNearPane = sceneCamera->GetNearPlane();
    const float oldFarPlane = sceneCamera->GetFarPlane();

    const std::array<float, CASCADE_COUNT> farPlane = { 10.f, sqrtf(MAX_SHADOW_DISTANCE) + 5.f };

    auto& shadowViewData = Renderer3D::ShaderStorages::ShadowViews.Data();

    for (int i = 0; i < CASCADE_COUNT; i++)
    {
        if (i != 0)
        {
            sceneCamera->SetNearPlane(oldNearPane + farPlane[i - 1]);
        }

        sceneCamera->SetFarPlane(farPlane[i]);
        sceneCamera->OnRender(0.f);

        const auto atlasSlot = m_Slots[i];
        const auto viewport = ShadowAtlas::GetViewport(atlasSlot);

        const auto frustumCorners = sceneCamera->GetFrustumCorners();

        const auto viewMatrix = BuildViewMatrix(frustumCorners, lightDirection);

        // The texel snap has to be against the tile the cascade actually lands in, not against a
        // separate resolution setting. It used to be GraphicsSettings::ShadowMapResolution, which
        // described a texture that no longer exists.
        const auto projectionMatrix = BuildProjectionMatrix(frustumCorners, viewMatrix, farPlane[i] * 0.5f, viewport.z);

        const auto viewProjection = projectionMatrix * viewMatrix;

        auto& view = m_Views[i];

        view.ViewProjection = viewProjection;
        view.ViewFrustum = Frustum::FromViewProjection(viewProjection);
        view.Origin = sceneCamera->GetParent()->GetTransform()->GetPosition();
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
        viewData.TileRect = ShadowAtlas::GetUvRect(atlasSlot);

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

    sceneCamera->SetNearPlane(oldNearPane);
    sceneCamera->SetFarPlane(oldFarPlane);
    sceneCamera->OnRender(0.f);

    return true;
}

std::vector<Rendering::ShadowView>& Rendering::ShadowCascades::GetViews()
{
    return m_Views;
}

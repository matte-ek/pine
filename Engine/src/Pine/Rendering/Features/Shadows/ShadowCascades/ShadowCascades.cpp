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
    // The cascades' pinned atlas tiles, reserved once at Setup. The owner is a token rather than a
    // Light because a level can swap its sun, or have none, without the reservation changing hands.
    const char m_TileOwner = 0;
    int m_Slots[CASCADE_COUNT] = {};
    bool m_SlotsReserved = false;

    // One view per cascade, kept across frames. See ShadowCascades::GetViews.
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

    // How far towards the light one caster's box reaches, in light-space z, or nothing when it
    // sits outside the column of light above the cascade box.
    std::optional<float> FitBoxNearZ(const Matrix4f& viewMatrix,
                                     const Vector3f& boundsMin, const Vector3f& boundsMax,
                                     const float minX, const float maxX,
                                     const float minY, const float maxY)
    {
        // Runs for every caster, per cascade, so the bounds come from centre and extents rather
        // than from eight transformed corners. See Math::TransformBounds.
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

    // Where the cascade's near plane goes, in light space.
    //
    // A caster *behind* the box on the light side still casts into it, so clipping at the box's own
    // front face makes shadows wink out as the camera turns. The near plane is pushed back to the
    // furthest caster whose light-space XY overlaps the box; the overlap test keeps one tall object
    // elsewhere in the level from stretching every cascade's depth range.
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
            if (!modelRenderer.GetModel() || !modelRenderer.GetCastShadows())
            {
                continue;
            }

            const auto& data = modelRenderer.GetRenderingHintData();

            if (const auto casterZ = FitBoxNearZ(viewMatrix, data.BoundsMin, data.BoundsMax, minX, maxX, minY, maxY))
            {
                nearZ = std::max(nearZ, casterZ.value());
            }
        }

        // Terrain too: a ridge outside the cascade box still shadows ground inside it.
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

        // glm::ortho takes zNear/zFar as distances along the view direction, while light space puts
        // everything visible at negative z, hence the negation. The margin keeps a caster sitting
        // exactly on the fitted plane from being clipped by it.
        constexpr float casterNearMargin = 0.5f;

        const float nearZ = FitCasterNearZ(viewMatrix, minX, maxX, minY, maxY, maxZ) + casterNearMargin;

        return glm::ortho(minX, maxX, minY, maxY, -nearZ, -minZ + farPlaneMargin);
    }
}

void Rendering::ShadowCascades::Setup()
{
    // Pinned before any light can compete for them. See ShadowAtlas::Reserve.
    m_SlotsReserved = ShadowAtlas::Reserve(ShadowAtlas::TileSize::Half, &m_TileOwner,
                                           CASCADE_COUNT, m_Slots);

    if (!m_SlotsReserved)
    {
        PError(fmt::format("Shadow atlas has no room for {} cascade tiles - directional shadows are off. "
                           "The atlas layout has to provide at least CASCADE_COUNT tiles of the largest class.",
                           CASCADE_COUNT));
    }
}

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

        // Texel-snapped against the tile the cascade actually lands in.
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

        // Always. The projection follows the camera's depth range, which changes on any camera
        // movement, so the tile cache would almost never hit.
        view.NeedsRender = true;

        auto& viewData = shadowViewData.Views[i];

        viewData.ViewProjection = viewProjection;
        viewData.TileRect = ShadowAtlas::GetUvRect(atlasSlot);

        // No texel scale or normal offset: front-face culling already provides the separation,
        // and stacking the two would peter-pan. Strength is 1, since a cascade never competes
        // for its tile and so never fades.
        viewData.Params = Vector4f(0.f, 0.f, 1.f, 0.f);
    }

    // The shader picks a cascade from the count, the same way it picks a cube face.
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

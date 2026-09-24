#include "TerrainRenderer.hpp"

#include "Pine/Assets/Material/Material.hpp"
#include "Pine/Performance/Performance.hpp"
#include "Pine/Rendering/Renderer3D/Renderer3D.hpp"
#include "Pine/Rendering/SceneProcessor/SceneLightsProcessor/SceneLightsProcessing.hpp"
#include "Pine/Rendering/SceneProcessor/SceneProcessor.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Components/Transform/Transform.hpp"
#include "Pine/World/Entity/Entity.hpp"

namespace
{
    using namespace Pine;

    // How far the finest level reaches, in chunk widths. Every level above it doubles that
    // distance, so the levels cover 2, 4, 8, 16 chunks out - geometric because the screen area a
    // chunk covers falls off the same way, which is what keeps a level change roughly the same size
    // on screen wherever it happens.
    //
    // Expressed in chunk widths rather than in world units so that a terrain built out of 16-unit
    // chunks and one built out of 256-unit chunks both switch level at the same apparent size.
    constexpr float LodReachInChunks = 2.f;

    // How many terrains were drawable last frame, which is what "a terrain was added or removed"
    // is derived from.
    std::size_t m_TerrainCount = 0;

    std::optional<Rendering::TerrainRenderer::BrushOverlay> m_BrushOverlay;

    // Terrain follows its entity's position and nothing else. A height field has one surface per
    // column by construction, so a rotated or scaled terrain is not something the data can
    // represent - and the collision unit, which cooks the same field into PhysX, cannot represent
    // it either. Better that the two agree on ignoring it than that the visible ground and the
    // ground you walk on disagree.
    Matrix4f GetChunkTransform(const TerrainRendererComponent* component, const TerrainChunk& chunk)
    {
        const auto terrain = component->GetTerrain();
        const auto entityPosition = component->GetParent()->GetTransform()->GetPosition();

        const auto chunkPosition = entityPosition + Vector3f(
            static_cast<float>(chunk.Coordinate.x) * terrain->GetChunkSize(),
            0.f,
            static_cast<float>(chunk.Coordinate.y) * terrain->GetChunkSize());

        return glm::translate(Matrix4f(1.f), chunkPosition);
    }

    // Centre of a chunk's box in world space, which is the point its lights are picked at. The
    // centre rather than a corner so that the chunk's whole footprint is equally represented - a
    // lamp just past one edge and one just past the opposite edge compete on equal terms.
    Vector3f GetChunkLightPosition(const Vector3f& entityPosition, const TerrainChunk& chunk)
    {
        return entityPosition + (chunk.BoundsMin + chunk.BoundsMax) * 0.5f;
    }

    // Distance to the nearest point of the box, which is zero for a viewer standing on the chunk.
    // Measuring to the chunk's origin instead would put a viewer in the middle of a large chunk a
    // long way from it and drop that chunk - the one filling the screen - to a coarse level.
    float GetDistanceToBounds(const Vector3f& position, const Vector3f& boundsMin, const Vector3f& boundsMax)
    {
        return glm::length(position - glm::clamp(position, boundsMin, boundsMax));
    }

    int SelectLodLevel(const Terrain* terrain, const float distance)
    {
        float reach = terrain->GetChunkSize() * LodReachInChunks;

        int level = 0;

        while (level + 1 < terrain->GetLodCount() && distance > reach)
        {
            level++;
            reach *= 2.f;
        }

        return level;
    }

    // Whether this terrain has been moved since its chunks were last lit, compared against the
    // position the slots were picked at.
    bool HasTerrainMoved(const TerrainRendererComponent& component, const Vector3f& entityPosition)
    {
        const auto& slotOrigin = component.GetLightSlotOrigin();

        return !slotOrigin.has_value() || slotOrigin.value() != entityPosition;
    }

    // Gives every chunk of one terrain the lights that reach it, skipping the chunks whose slots
    // are still good.
    //
    // Scene-level work: the slots depend on where the lights and the chunks are and on nothing
    // about who is looking, so every viewer in the frame shares one answer.
    void AssignChunkLightSlots(const Rendering::SceneProcessor::SceneProcessorContext& sceneContext,
                               Terrain* terrain,
                               const Vector3f& entityPosition,
                               const bool hasTerrainMoved)
    {
        for (auto& chunk : terrain->GetChunks())
        {
            if (chunk.LightSlots.HasComputedData && !hasTerrainMoved && !sceneContext.LightSetChanged)
            {
                continue;
            }

            Rendering::SceneProcessor::Lights::AssignSlots(
                sceneContext,
                GetChunkLightPosition(entityPosition, chunk),
                chunk.LightSlots);
        }
    }

    // The materials a terrain's splat channels blend, in channel order. Read once per terrain
    // rather than once per chunk: every chunk of one terrain blends the same four.
    std::array<Material*, Renderer3D::Specifications::TerrainLayers::COUNT> GetTerrainLayers(const Terrain* terrain)
    {
        std::array<Material*, Renderer3D::Specifications::TerrainLayers::COUNT> layers{};

        for (int layer = 0; layer < Renderer3D::Specifications::TerrainLayers::COUNT; layer++)
        {
            layers[layer] = terrain->GetLayer(layer);
        }

        return layers;
    }

    void RenderTerrain(const TerrainRendererComponent* component,
                       const Rendering::TerrainRenderer::TerrainView& view)
    {
        const auto terrain = component->GetTerrain();
        const auto entityPosition = component->GetParent()->GetTransform()->GetPosition();

        const auto layers = GetTerrainLayers(terrain);
        const auto splatTransform = terrain->GetSplatTransform();

        // Packed once per terrain rather than per chunk: every chunk of one terrain reads the same
        // ring, and the coordinate it is expressed in - terrain-local - is the same one the chunk
        // meshes carry as their uv.
        std::optional<Vector4f> brushRing;

        if (m_BrushOverlay.has_value() && m_BrushOverlay->Terrain == terrain->GetUId())
        {
            brushRing = Vector4f(m_BrushOverlay->Centre.x, m_BrushOverlay->Centre.y,
                                 m_BrushOverlay->Radius, m_BrushOverlay->RingWidth);
        }

        for (auto& chunk : terrain->GetChunks())
        {
            // Chunk bounds are terrain-local, and the terrain sits wherever its entity does.
            const auto boundsMin = chunk.BoundsMin + entityPosition;
            const auto boundsMax = chunk.BoundsMax + entityPosition;

            if (!view.ViewFrustum.Intersects(boundsMin, boundsMax))
            {
                if (view.Statistics != nullptr)
                {
                    view.Statistics->CulledTerrainChunkCount++;
                }

                continue;
            }

            const auto lodLevel = SelectLodLevel(terrain, GetDistanceToBounds(view.LodOrigin, boundsMin, boundsMax));
            const auto mesh = terrain->GetChunkMesh(chunk, lodLevel);

            // Null until Prepare has built it, which is the state every chunk of a terrain that was
            // only just assigned to the component is in for the rest of this frame.
            if (mesh == nullptr)
            {
                continue;
            }

            if (view.Statistics != nullptr)
            {
                view.Statistics->VisibleTerrainChunkCount++;
            }

            Renderer3D::PrepareTerrainChunk(mesh, layers, terrain->GetSplatMap(), splatTransform,
                                            brushRing.has_value() ? &brushRing.value() : nullptr);

            // The skirt is the tail of the chunk's index buffer, so a pass that does not want it
            // draws the ground's indices and stops.
            const auto indexCount = view.DrawSkirts ? 0u : terrain->GetChunkGroundIndexCount(lodLevel);

            Renderer3D::RenderMesh(GetChunkTransform(component, chunk), &chunk.LightSlots, 0x00, indexCount);
        }
    }
}

void Pine::Rendering::TerrainRenderer::Setup()
{
}

void Pine::Rendering::TerrainRenderer::Shutdown()
{
    m_TerrainCount = 0;
    m_BrushOverlay.reset();
}

void Pine::Rendering::TerrainRenderer::SetBrushOverlay(const std::optional<BrushOverlay>& overlay)
{
    m_BrushOverlay = overlay;
}

const std::optional<Pine::Rendering::TerrainRenderer::BrushOverlay>& Pine::Rendering::TerrainRenderer::GetBrushOverlay()
{
    return m_BrushOverlay;
}

void Pine::Rendering::TerrainRenderer::Prepare(SceneProcessor::SceneProcessorContext& sceneContext)
{
    PINE_PF_SCOPE();

    bool hasAnyTerrainChanged = false;
    std::size_t terrainCount = 0;

    for (auto& terrainRenderer : Components::Get<TerrainRendererComponent>())
    {
        const auto terrain = terrainRenderer.GetTerrain();

        if (terrain == nullptr)
        {
            continue;
        }

        terrainCount++;

        const auto entityPosition = terrainRenderer.GetParent()->GetTransform()->GetPosition();
        const bool hasMoved = HasTerrainMoved(terrainRenderer, entityPosition);

        // Not folded into the condition below: the rebuild has to run whatever the flag already
        // says, and a short-circuit would skip it for every terrain after the first that changed.
        const bool hasRebuiltMeshes = terrain->RebuildDirtyChunkMeshes();

        // Layer weights change what the ground looks like and not what shape it is, so this does
        // not feed the terrain-changed signal below: the cached shadow tiles hold depth, which a
        // repaint cannot move.
        terrain->RebuildDirtySplatMap();

        hasAnyTerrainChanged = hasAnyTerrainChanged || hasRebuiltMeshes || hasMoved;

        AssignChunkLightSlots(sceneContext, terrain, entityPosition, hasMoved);

        terrainRenderer.SetLightSlotOrigin(entityPosition);
    }

    // A count that did not change is not proof the set did not - the same trade the scene
    // processor makes for its casters, and for the same reason: per-terrain identity tracking is a
    // cost every frame to catch a case that costs one stale frame.
    hasAnyTerrainChanged = hasAnyTerrainChanged || terrainCount != m_TerrainCount;
    m_TerrainCount = terrainCount;

    sceneContext.TerrainChanged = hasAnyTerrainChanged;
}

void Pine::Rendering::TerrainRenderer::BeginPass(RenderingContext& context)
{
    context.Statistics.VisibleTerrainChunkCount = 0;
    context.Statistics.CulledTerrainChunkCount = 0;
}

void Pine::Rendering::TerrainRenderer::Render(const TerrainView& view)
{
    PINE_PF_SCOPE();

    for (const auto& terrainRenderer : Components::Get<TerrainRendererComponent>())
    {
        if (terrainRenderer.GetTerrain() == nullptr)
        {
            continue;
        }

        RenderTerrain(&terrainRenderer, view);
    }
}

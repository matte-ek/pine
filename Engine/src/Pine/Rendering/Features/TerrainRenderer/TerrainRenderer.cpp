#include "TerrainRenderer.hpp"

#include "Pine/Rendering/Renderer3D/Renderer3D.hpp"
#include "Pine/World/Components/TerrainRenderer/TerrainRendererComponent.hpp"
#include "Pine/World/Components/Transform/Transform.hpp"
#include "Pine/World/Entity/Entity.hpp"

namespace
{
    constexpr float TERRAIN_SCALE = 1.f;

    using namespace Pine;

    Matrix4f ComputeChunkTransform(const Transform* parent, Vector2i localGridPosition)
    {
        auto transform = Matrix4f(1.f);

        auto worldPosition = parent->GetPosition();

        worldPosition += Vector3f(
            localGridPosition.x * TERRAIN_CHUNK_SIZE,
            0.f,
            localGridPosition.y * TERRAIN_CHUNK_SIZE);

        /*
        worldPosition += Vector3f(
            TERRAIN_CHUNK_SIZE / 2.f,
            0.f,
            TERRAIN_CHUNK_SIZE / 2.f
        );
        */

        transform = glm::translate(transform, worldPosition);
        transform = glm::scale(transform, Vector3f(TERRAIN_SCALE));

        return transform;
    }

    void RenderChunk(const TerrainRendererComponent* component, const TerrainChunk* terrainChunk)
    {
        const auto transform = ComputeChunkTransform(component->GetParent()->GetTransform(), terrainChunk->Position);

        Renderer3D::PrepareMesh(terrainChunk->ChunkMesh, nullptr);
        Renderer3D::RenderMesh(transform);
    }
}

void Rendering::TerrainRenderer::Setup()
{
}

void Rendering::TerrainRenderer::Shutdown()
{
}

void Rendering::TerrainRenderer::Render(const TerrainRendererComponent* terrainRendererComponent)
{
    auto terrain = terrainRendererComponent->GetTerrain();

    for (const auto& chunk : terrain->GetChunks())
    {
        if (!chunk.IsReady)
        {
            continue;
        }

        RenderChunk(terrainRendererComponent, &chunk);
    }
}

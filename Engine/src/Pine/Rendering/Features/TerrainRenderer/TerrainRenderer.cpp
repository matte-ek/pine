#include "TerrainRenderer.hpp"

#include "Pine/Rendering/Renderer3D/Renderer3D.hpp"
#include "Pine/World/Components/TerrainRenderer/TerrainRendererComponent.hpp"
#include "Pine/World/Components/Transform/Transform.hpp"
#include "Pine/World/Entity/Entity.hpp"

namespace
{
    constexpr float TERRAIN_SCALE = 1.f;

    using namespace Pine;

    Camera* m_SceneCamera = nullptr;

    void RenderChunk(const TerrainRendererComponent* component, const TerrainChunk* terrainChunk)
    {
        const auto transformComponent = component->GetParent()->GetTransform();

        auto worldPosition = transformComponent->GetPosition();
        auto transform = Matrix4f(1.f);

        worldPosition += Vector3f(
            terrainChunk->Position.x * TERRAIN_CHUNK_SIZE - 1,
            0.f,
            terrainChunk->Position.y * TERRAIN_CHUNK_SIZE - 1);

        transform = glm::translate(transform, worldPosition);
        transform = glm::scale(transform, Vector3f(TERRAIN_SCALE));

        const auto useLowPoly = glm::distance2(
            m_SceneCamera->GetParent()->GetTransform()->GetPosition(),
            worldPosition + Vector3f(TERRAIN_CHUNK_SIZE * 0.5f, 0.f, TERRAIN_CHUNK_SIZE * 0.5f)) > 3000.f;
        auto mesh = useLowPoly ? terrainChunk->ChunkMeshLowPoly : terrainChunk->ChunkMesh;

        Renderer3D::PrepareMesh(mesh, terrainChunk->Material.Get());
        Renderer3D::RenderMesh(transform);
    }
}

void Rendering::TerrainRenderer::Setup()
{
}

void Rendering::TerrainRenderer::Shutdown()
{
}

void Rendering::TerrainRenderer::NewFrame(Camera* sceneCamera)
{
    m_SceneCamera = sceneCamera;
}

void Rendering::TerrainRenderer::Render(const TerrainRendererComponent* terrainRendererComponent)
{
    auto terrain = terrainRendererComponent->GetTerrain();

    assert(m_SceneCamera);

    for (const auto& chunk : terrain->GetChunks())
    {
        if (!chunk.IsReady)
        {
            terrain->GenerateMesh();

            if (!chunk.IsReady)
            {
                continue;
            }
        }

        RenderChunk(terrainRendererComponent, &chunk);
    }
}

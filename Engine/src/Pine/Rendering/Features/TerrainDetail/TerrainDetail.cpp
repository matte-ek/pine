#include "TerrainDetail.hpp"

#include "Pine/Assets/Material/Material.hpp"
#include "Pine/Assets/Terrain/Terrain.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Performance/Performance.hpp"
#include "Pine/Rendering/Renderer3D/Renderer3D.hpp"
#include "Pine/Rendering/Renderer3D/ShaderStorages.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Components/TerrainRenderer/TerrainRendererComponent.hpp"
#include "Pine/World/Components/Transform/Transform.hpp"
#include "Pine/World/Entity/Entity.hpp"

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <vector>

namespace
{
    using namespace Pine;

    // How many placement sets may be generated in one frame. Generation is CPU work that grows with
    // the density, and a camera arriving somewhere new - a level load, a teleport - would otherwise
    // generate everything around it at once. The nearest sets go first, so whatever is missing for
    // a few frames is what is furthest away and most faded.
    constexpr int MaximumGenerationsPerFrame = 4;

    // Placements are kept until their chunk is this many times the draw distance away, so a camera
    // moving back and forth across the edge of the draw distance does not regenerate the same
    // chunk every time it crosses.
    constexpr float KeepDistanceScale = 1.25f;

    // Where copies start shrinking into the ground, as a fraction of the draw distance.
    constexpr float FadeStartFraction = 0.8f;

    // One detail type within one chunk of one terrain.
    struct BatchKey
    {
        UId Terrain;
        Vector2i Chunk{};
        int DetailType = 0;

        bool operator==(const BatchKey& other) const
        {
            return Terrain == other.Terrain && Chunk == other.Chunk && DetailType == other.DetailType;
        }
    };

    struct BatchKeyHash
    {
        std::size_t operator()(const BatchKey& key) const noexcept
        {
            std::size_t hash = std::hash<UId>{}(key.Terrain);

            hash = hash * 31 + std::hash<int>{}(key.Chunk.x);
            hash = hash * 31 + std::hash<int>{}(key.Chunk.y);
            hash = hash * 31 + std::hash<int>{}(key.DetailType);

            return hash;
        }
    };

    // The placements of one BatchKey, on the GPU.
    struct Batch
    {
        // Null when the chunk grows none of this detail type, which is kept as a batch of its own
        // so that bare ground is not regenerated every frame.
        Graphics::IStorageBuffer* Instances = nullptr;
        int InstanceCount = 0;

        // The chunk's TerrainChunk::DetailRevision these were generated from.
        std::uint64_t Revision = 0;

        // Whether a camera was near enough to keep this during the current Prepare.
        bool IsInRange = false;
    };

    // A batch that is in range but missing or out of date.
    struct PendingGeneration
    {
        BatchKey Key;
        const Terrain* SourceTerrain = nullptr;
        const TerrainChunk* Chunk = nullptr;
        float Distance = 0.f;
    };

    std::unordered_map<BatchKey, Batch, BatchKeyHash> m_Batches;

    // Where every camera that draws the scene is this frame. Detail is kept around all of them,
    // so a second viewport looking at another part of the terrain has its own detail too.
    std::vector<Vector3f> m_CameraPositions;

    void GatherCameraPositions()
    {
        m_CameraPositions.clear();

        for (const auto context : RenderManager::GetRenderingContexts())
        {
            if (context == nullptr || !context->Active || !context->UseRenderPipeline || context->SceneCamera == nullptr)
            {
                continue;
            }

            m_CameraPositions.push_back(context->SceneCamera->GetParent()->GetTransform()->GetPosition());
        }
    }

    float GetDistanceToBounds(const Vector3f& position, const Vector3f& boundsMin, const Vector3f& boundsMax)
    {
        return glm::length(position - glm::clamp(position, boundsMin, boundsMax));
    }

    float GetNearestCameraDistance(const Vector3f& boundsMin, const Vector3f& boundsMax)
    {
        float nearest = std::numeric_limits<float>::max();

        for (const auto& cameraPosition : m_CameraPositions)
        {
            nearest = std::min(nearest, GetDistanceToBounds(cameraPosition, boundsMin, boundsMax));
        }

        return nearest;
    }

    void ReleaseBatch(Batch& batch)
    {
        if (batch.Instances != nullptr)
        {
            Graphics::GetGraphicsAPI()->DestroyStorageBuffer(batch.Instances);
        }

        batch.Instances = nullptr;
        batch.InstanceCount = 0;
    }

    void Generate(const PendingGeneration& pending)
    {
        auto& batch = m_Batches[pending.Key];

        batch.IsInRange = true;

        // Two entities placing the same terrain queue the same batch twice. Revisions start at 1,
        // so a batch that has just been created never matches.
        if (batch.Revision == pending.Chunk->DetailRevision)
        {
            return;
        }

        const auto placements = pending.SourceTerrain->GenerateDetailInstances(*pending.Chunk, pending.Key.DetailType);

        batch.Revision = pending.Chunk->DetailRevision;
        batch.InstanceCount = static_cast<int>(placements.size());

        if (placements.empty())
        {
            ReleaseBatch(batch);
            return;
        }

        std::vector<Renderer3D::ShaderStorages::TerrainDetailInstanceData> instanceData;

        instanceData.reserve(placements.size());

        for (const auto& placement : placements)
        {
            instanceData.push_back({
                Vector4f(placement.Position, placement.Scale),
                Vector4f(std::cos(placement.Yaw), std::sin(placement.Yaw), 0.f, 0.f)
            });
        }

        const auto size = instanceData.size() * sizeof(Renderer3D::ShaderStorages::TerrainDetailInstanceData);

        // A repaint usually changes the count a little either way, so a buffer that is large enough
        // is reused rather than reallocated for every stroke.
        if (batch.Instances != nullptr && batch.Instances->GetSize() < size)
        {
            Graphics::GetGraphicsAPI()->DestroyStorageBuffer(batch.Instances);
            batch.Instances = nullptr;
        }

        if (batch.Instances == nullptr)
        {
            batch.Instances = Graphics::GetGraphicsAPI()->CreateStorageBuffer();
            batch.Instances->Create(size, Graphics::BufferUsageHint::StaticDraw);
        }

        batch.Instances->UploadData(instanceData.data(), size, 0);
    }

    // How far a copy of this detail type can reach out of its chunk's box: the model's furthest
    // extent from its origin at the largest scale, in any direction.
    float GetDetailReach(const TerrainDetailType& detailType, const Model* model)
    {
        const auto extent = glm::max(glm::abs(model->GetBoundingBoxMin()), glm::abs(model->GetBoundingBoxMax()));

        return std::max({ extent.x, extent.y, extent.z }) * detailType.ScaleMax;
    }

    void RenderTerrain(const TerrainRendererComponent& component, RenderingContext& context, const Vector3f& cameraPosition)
    {
        auto* graphicsApi = Graphics::GetGraphicsAPI();

        const auto terrain = component.GetTerrain();
        const auto entityPosition = component.GetParent()->GetTransform()->GetPosition();

        // Terrain follows its entity's position and nothing else - see TerrainRenderer.
        const auto terrainTransform = glm::translate(Matrix4f(1.f), entityPosition);

        const auto& detailTypes = terrain->GetDetailTypes();

        // Detail type outermost, so each mesh and material is prepared once and then drawn for
        // every chunk that has it.
        for (int detailIndex = 0; detailIndex < static_cast<int>(detailTypes.size()); detailIndex++)
        {
            const auto& detailType = detailTypes[detailIndex];
            const auto model = detailType.DetailModel.Get();

            if (model == nullptr)
            {
                continue;
            }

            const Vector2f fadeDistances = { detailType.DrawDistance * FadeStartFraction, detailType.DrawDistance };
            const float reach = GetDetailReach(detailType, model);

            for (const auto mesh : model->GetMeshes())
            {
                if (!Renderer3D::PrepareTerrainDetailMesh(mesh))
                {
                    continue;
                }

                const auto material = Renderer3D::ResolveMaterial(mesh);
                const bool isTwoSided = material != nullptr && material->GetRenderFace() == MaterialRenderFace::Both;

                graphicsApi->SetFaceCullingEnabled(!isTwoSided);

                for (auto& chunk : terrain->GetChunks())
                {
                    const auto batch = m_Batches.find({ terrain->GetUId(), chunk.Coordinate, detailIndex });

                    if (batch == m_Batches.end() || batch->second.InstanceCount == 0)
                    {
                        continue;
                    }

                    const auto chunkMin = chunk.BoundsMin + entityPosition;
                    const auto chunkMax = chunk.BoundsMax + entityPosition;

                    if (GetDistanceToBounds(cameraPosition, chunkMin, chunkMax) > detailType.DrawDistance)
                    {
                        continue;
                    }

                    if (!context.ViewFrustum.Intersects(chunkMin - Vector3f(reach), chunkMax + Vector3f(reach)))
                    {
                        continue;
                    }

                    Renderer3D::RenderTerrainDetail(terrainTransform,
                                                    &chunk.LightSlots,
                                                    batch->second.Instances,
                                                    batch->second.InstanceCount,
                                                    fadeDistances);

                    context.Statistics.TerrainDetailInstanceCount += batch->second.InstanceCount;
                }
            }
        }

        // The scene pass draws everything else with back faces culled.
        graphicsApi->SetFaceCullingEnabled(true);
    }
}

void Pine::Rendering::TerrainDetail::Shutdown()
{
    for (auto& [key, batch] : m_Batches)
    {
        ReleaseBatch(batch);
    }

    m_Batches.clear();
    m_CameraPositions.clear();
}

void Pine::Rendering::TerrainDetail::Prepare()
{
    PINE_PF_SCOPE();

    GatherCameraPositions();

    for (auto& [key, batch] : m_Batches)
    {
        batch.IsInRange = false;
    }

    std::vector<PendingGeneration> pending;

    for (const auto& terrainRenderer : Components::Get<TerrainRendererComponent>())
    {
        const auto terrain = terrainRenderer.GetTerrain();

        if (terrain == nullptr)
        {
            continue;
        }

        const auto entityPosition = terrainRenderer.GetParent()->GetTransform()->GetPosition();
        const auto& detailTypes = terrain->GetDetailTypes();

        for (const auto& chunk : terrain->GetChunks())
        {
            const float distance = GetNearestCameraDistance(chunk.BoundsMin + entityPosition, chunk.BoundsMax + entityPosition);

            for (int detailIndex = 0; detailIndex < static_cast<int>(detailTypes.size()); detailIndex++)
            {
                const auto& detailType = detailTypes[detailIndex];

                if (detailType.DetailModel.Get() == nullptr || detailType.Density <= 0.f)
                {
                    continue;
                }

                if (distance > detailType.DrawDistance * KeepDistanceScale)
                {
                    continue;
                }

                const BatchKey key = { terrain->GetUId(), chunk.Coordinate, detailIndex };
                const auto existing = m_Batches.find(key);

                bool isCurrent = false;

                if (existing != m_Batches.end())
                {
                    existing->second.IsInRange = true;
                    isCurrent = existing->second.Revision == chunk.DetailRevision;
                }

                // Only generated once it is actually within the draw distance; the wider keep
                // distance is for holding on to what already exists.
                if (!isCurrent && distance <= detailType.DrawDistance)
                {
                    pending.push_back({ key, terrain, &chunk, distance });
                }
            }
        }
    }

    // Whatever no camera is near any more, and whatever belongs to a terrain, chunk or detail type
    // that no longer exists, was not marked above.
    for (auto batch = m_Batches.begin(); batch != m_Batches.end();)
    {
        if (batch->second.IsInRange)
        {
            ++batch;
            continue;
        }

        ReleaseBatch(batch->second);
        batch = m_Batches.erase(batch);
    }

    std::sort(pending.begin(), pending.end(), [](const PendingGeneration& a, const PendingGeneration& b)
    {
        return a.Distance < b.Distance;
    });

    const auto generationCount = std::min(pending.size(), static_cast<std::size_t>(MaximumGenerationsPerFrame));

    for (std::size_t index = 0; index < generationCount; index++)
    {
        Generate(pending[index]);
    }
}

void Pine::Rendering::TerrainDetail::Render(RenderingContext& context)
{
    PINE_PF_SCOPE();

    if (context.SceneCamera == nullptr || m_Batches.empty())
    {
        return;
    }

    const auto cameraPosition = context.SceneCamera->GetParent()->GetTransform()->GetPosition();

    for (const auto& terrainRenderer : Components::Get<TerrainRendererComponent>())
    {
        if (terrainRenderer.GetTerrain() == nullptr)
        {
            continue;
        }

        RenderTerrain(terrainRenderer, context, cameraPosition);
    }
}

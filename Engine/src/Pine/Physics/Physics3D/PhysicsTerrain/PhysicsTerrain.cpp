#include "PhysicsTerrain.hpp"
#include "physx/PxPhysicsAPI.h"
#include "Pine/Assets/Terrain/Terrain.hpp"
#include "Pine/Physics/Physics3D/Physics3D.hpp"

using namespace Pine;
using namespace physx;

namespace
{
    void CookTerrainData(TerrainChunkPhysicsData* data, const float* heightMap)
    {
        static PxHeightFieldSample samples[TERRAIN_CHUNK_VERTEX_COUNT * TERRAIN_CHUNK_VERTEX_COUNT];

        for (int y = 0; y < TERRAIN_CHUNK_VERTEX_COUNT; y++)
        {
            for (int x = 0; x < TERRAIN_CHUNK_VERTEX_COUNT; x++)
            {
                PxHeightFieldSample& s = samples[y * TERRAIN_CHUNK_VERTEX_COUNT + x];

                s.materialIndex0 = 0;
                s.materialIndex1 = 0;
                s.height = static_cast<int>(heightMap[y * TERRAIN_CHUNK_VERTEX_COUNT + x] * 100);

                s.clearTessFlag();
            }
        }

        PxHeightFieldDesc desc;

        desc.nbRows = TERRAIN_CHUNK_VERTEX_COUNT;
        desc.nbColumns = TERRAIN_CHUNK_VERTEX_COUNT;
        desc.samples.data = samples;
        desc.samples.stride = sizeof(PxHeightFieldSample);
        desc.format = PxHeightFieldFormat::eS16_TM;

        PxDefaultMemoryOutputStream output;

        PxCookHeightField(desc, output);

        data->PhysicsHeightFieldDataSize = output.getSize();
        data->PhysicsHeightFieldData = malloc(data->PhysicsHeightFieldDataSize);

        memcpy(data->PhysicsHeightFieldData, output.getData(), data->PhysicsHeightFieldDataSize);
    }

    void CreateHeightfield(TerrainChunkPhysicsData* data)
    {
        PxDefaultMemoryInputData inputStream(static_cast<PxU8*>(data->PhysicsHeightFieldData), data->PhysicsHeightFieldDataSize);

        data->PhysicsHeightField = Physics3D::GetPhysics()->createHeightField(inputStream);
    }
}

void Physics3D::Terrain::Prepare(TerrainChunkPhysicsData* data, const float* heightMap)
{
    CookTerrainData(data, heightMap);
    CreateHeightfield(data);
}

void Physics3D::Terrain::Destroy(TerrainChunkPhysicsData* data)
{
    free(data->PhysicsHeightFieldData);

    data->PhysicsHeightFieldData = nullptr;
}

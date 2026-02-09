#pragma once

namespace Pine
{
    struct TerrainChunkPhysicsData;
}

namespace Pine::Physics3D::Terrain
{
    void Prepare(TerrainChunkPhysicsData* data, const float* heightMap);
    void Destroy(TerrainChunkPhysicsData* data);
}

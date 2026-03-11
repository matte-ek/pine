#pragma once
#include <array>

#include "Pine/Assets/Terrain/Terrain.hpp"

namespace Pine
{
    struct TerrainChunkPhysicsData;
}

namespace Pine::Physics3D::Terrain
{
    void Prepare(TerrainChunkPhysicsData* data, const std::array<float, TERRAIN_SQUARE_SIZE>& heightMap);
    void Destroy(TerrainChunkPhysicsData* data);
}

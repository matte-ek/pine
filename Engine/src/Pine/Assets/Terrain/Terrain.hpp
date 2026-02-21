#pragma once
#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Core/Math/Math.hpp"
#include "Pine/Graphics/Graphics.hpp"

namespace Pine
{
    class Mesh;
    class Texture2D;

    constexpr int TERRAIN_CHUNK_SIZE = 128;
    constexpr int TERRAIN_CHUNK_VERTEX_COUNT = 256;

    struct TerrainChunkPhysicsData
    {
        void* PhysicsHeightField = nullptr;

        void* PhysicsHeightFieldData = nullptr;
        size_t PhysicsHeightFieldDataSize = 0;
    };

    struct TerrainChunk
    {
        Vector2i Position;

        AssetHandle<Texture2D> HeightmapTexture;

        Mesh* ChunkMesh = nullptr;

        TerrainChunkPhysicsData PhysicsData;

        bool IsReady = false;
    };

    class Terrain : public Asset
    {
    private:
        std::vector<TerrainChunk> m_Chunks;
    public:
        explicit Terrain();

        void CreateChunk(Vector2i position, Texture2D* heightMap = nullptr);
        void Generate();

        std::vector<TerrainChunk>& GetChunks();

        void Dispose() override;
    };
}

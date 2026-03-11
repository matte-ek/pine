#pragma once
#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Core/Math/Math.hpp"
#include "Pine/Graphics/Graphics.hpp"

namespace Pine
{
    class Material;
    class Mesh;
    class Texture2D;

    constexpr int TERRAIN_CHUNK_SIZE = 64;
    constexpr int TERRAIN_CHUNK_VERTEX_COUNT = 256;
    constexpr int TERRAIN_CHUNK_VERTEX_COUNT_LP = 64;

    constexpr int TERRAIN_SQUARE_SIZE = (TERRAIN_CHUNK_VERTEX_COUNT + 2) * (TERRAIN_CHUNK_VERTEX_COUNT + 2);
    constexpr int TERRAIN_SQUARE_SIZE_LP = (TERRAIN_CHUNK_VERTEX_COUNT_LP + 2) * (TERRAIN_CHUNK_VERTEX_COUNT_LP + 2);

    struct TerrainChunkPhysicsData
    {
        void* PhysicsHeightField = nullptr;
        void* PhysicsHeightFieldData = nullptr;
        size_t PhysicsHeightFieldDataSize = 0;
    };

    struct TerrainChunk
    {
        Vector2i Position;

        std::array<float, TERRAIN_SQUARE_SIZE> HeightData;

        AssetHandle<Material> Material;

        Mesh* ChunkMesh = nullptr;
        Mesh* ChunkMeshLowPoly = nullptr;

        TerrainChunkPhysicsData PhysicsData;

        bool IsReady = false;
    };

    struct TerrainPerlinSettings
    {
        std::int32_t Seed = 123456u;

        float Layer0_CoordinateScale = 0.004f;
        float Layer1_CoordinateScale = 0.004f;
        float Layer2_CoordinateScale = 0.004f;

        std::int32_t Layer0_Octaves = 8;
        std::int32_t Layer1_Octaves = 8;
        std::int32_t Layer2_Octaves = 8;

        float Layer0_Scale = 5.f;
        float Layer1_Scale = 5.f;
        float Layer2_Scale = 5.f;

        float Layer2_Cutoff = 0.f;
    };

    class Terrain : public Asset
    {
    private:
        std::vector<TerrainChunk> m_Chunks;

        bool LoadAssetData(const ByteSpan& span) override;
        ByteSpan SaveAssetData() override;

        struct TerrainSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_ARRAY(Chunks);
        };

        struct TerrainChunkSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_PRIMITIVE(Position, Serialization::DataType::Vec2);
            PINE_SERIALIZE_ARRAY_FIXED(HeightData, float);
        };
    public:
        explicit Terrain();

        void CreateChunk(Vector2i position);
        void GenerateMesh();

        static void GenerateFromPerlinNoise(TerrainChunk& chunk, const TerrainPerlinSettings& perlinSettings);

        std::vector<TerrainChunk>& GetChunks();

        void Dispose() override;
    };
}

#include "Terrain.hpp"

#include <stb/stb_image.h>
#include <physx/cooking/PxCooking.h>
#include <physx/extensions/PxDefaultStreams.h>
#include <physx/geometry/PxHeightFieldSample.h>

#include "PerlinNoise.hpp"
#include "Pine/Assets/Mesh/Mesh.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Core/Serialization/Json/SerializationJson.hpp"
#include "Pine/Performance/Performance.hpp"
#include "Pine/Physics/Physics3D/PhysicsTerrain/PhysicsTerrain.hpp"

namespace
{
    using namespace Pine;

    float* GetHeightmapData(int size, Texture2D* texture2D)
    {
        int width, height, channels;

        const auto heightMapData = stbi_load(texture2D->GetFilePath().string().c_str(), &width, &height, &channels, 1);

        if (heightMapData == nullptr)
        {
            stbi_image_free(heightMapData);
            return nullptr;
        }

        if (width != size || height != size)
        {
            stbi_image_free(heightMapData);
            return nullptr;
        }

        auto heightMap = new float[size * size];

        for (int y = 0; y < height;y++)
        {
            for (int x = 0; x < width; x++)
            {
                float pixelHeight = static_cast<float>(heightMapData[y * width + x]) / 255.f;

                pixelHeight -= 0.5f;
                pixelHeight *= 2.f;

                heightMap[y * width + x] = pixelHeight * 10.f;
            }
        }

        return heightMap;
    }

    std::array<float, TERRAIN_SQUARE_SIZE> GeneratePerlinNoiseHeightmapData(Vector2f offset, const TerrainPerlinSettings& perlinSettings)
    {
        const siv::PerlinNoise perlin{ static_cast<std::uint32_t>(perlinSettings.Seed) };

        std::array<float, TERRAIN_SQUARE_SIZE> buff = {};

        for (int y = 0; y < TERRAIN_CHUNK_VERTEX_COUNT; y++)
        {
            for (int x = 0; x < TERRAIN_CHUNK_VERTEX_COUNT;x++)
            {

                buff[y * TERRAIN_CHUNK_VERTEX_COUNT + x] =
                    perlin.octave2D_11(
                        (offset.x + x) * perlinSettings.Layer0_CoordinateScale,
                        (offset.y + y) * perlinSettings.Layer0_CoordinateScale,
                        perlinSettings.Layer0_Octaves)
                    * perlinSettings.Layer0_Scale;

                buff[y * TERRAIN_CHUNK_VERTEX_COUNT + x] +=
                    perlin.octave2D_11(
                        (offset.x + x) * perlinSettings.Layer1_CoordinateScale,
                        (offset.y + y) * perlinSettings.Layer1_CoordinateScale,
                        perlinSettings.Layer1_Octaves)
                    * perlinSettings.Layer1_Scale;

                if (buff[y * TERRAIN_CHUNK_VERTEX_COUNT + x] > perlinSettings.Layer2_Cutoff)
                {
                    buff[y * TERRAIN_CHUNK_VERTEX_COUNT + x] +=
                        perlin.octave2D_11(
                            (offset.x + x) * perlinSettings.Layer2_CoordinateScale,
                            (offset.y + y) * perlinSettings.Layer2_CoordinateScale,
                            perlinSettings.Layer2_Octaves)
                        * perlinSettings.Layer2_Scale;
                }
            }
        }

        return buff;
    }

    float DownsampleBlock(const std::array<float, TERRAIN_SQUARE_SIZE>& src, int inputX, int inputY)
    {
        constexpr auto stride = TERRAIN_CHUNK_VERTEX_COUNT;

        float sum = 0;

        for (int y = 0;y < 4;y++)
        {
            for (int x = 0;x < 4;x++)
            {
                float height = src[(inputY + y) * stride + (inputX + x)];

                sum += height;
            }
        }

        return sum / 16.f;
    }

    std::array<float, TERRAIN_SQUARE_SIZE_LP> DownscaleHeightMap(const std::array<float, TERRAIN_SQUARE_SIZE>& heightMap)
    {
        std::array<float, TERRAIN_SQUARE_SIZE_LP> buff = {};

        for (int y = 0; y < TERRAIN_CHUNK_VERTEX_COUNT_LP; y++)
        {
            for (int x = 0; x < TERRAIN_CHUNK_VERTEX_COUNT_LP; x++)
            {
                buff[y * TERRAIN_CHUNK_VERTEX_COUNT_LP + x] = DownsampleBlock(heightMap, x * 4, y * 4);
            }
        }

        return buff;
    }

    template<size_t TerrainSize>
    float GetHeight(const std::array<float, TerrainSize>& heightMap, int x, int z)
    {
        if (0 > x) x = 0;
        if (0 > z) z = 0;

        int arrayIndex = z * TerrainSize + x;
        if (arrayIndex >= heightMap.size())
        {
            arrayIndex = heightMap.size() - 1;
        }

        return heightMap[arrayIndex];
    }

    template<size_t TerrainSize>
    Vector3f ComputeNormal(const std::array<float, TerrainSize>& heightMap, int x, int z)
    {
        float l = GetHeight(heightMap, x - 1, z);
        float r = GetHeight(heightMap, x + 1, z);
        float d = GetHeight(heightMap, x, z - 1);
        float u = GetHeight(heightMap, x, z + 1);

        return glm::normalize(Vector3f(l - r, 2.f, d - u));
    }

    template<size_t TerrainSize>
    void GenerateMeshFromHeightmap(
        Mesh* mesh,
        const std::array<float, (TerrainSize + 2) * (TerrainSize + 2)>& heightMap)
    {
        std::uint32_t totalIndexCount = 6 * (TerrainSize - 1) * (TerrainSize - 1);
        std::uint32_t totalVertexCount = TerrainSize * TerrainSize;

        auto vertices   = new Vector3f[totalVertexCount];
        auto normals    = new Vector3f[totalVertexCount];
        auto uvs        = new Vector2f[totalVertexCount];
        auto indices    = new std::uint32_t[totalIndexCount];

        std::uint32_t count = 0;

        for (int z = 0; z < TerrainSize; z++)
        {
            for (int x = 0; x < TerrainSize; x++)
            {
                vertices[count].x = static_cast<float>(x) / (TerrainSize - 1) * TERRAIN_CHUNK_SIZE;
                vertices[count].y = heightMap[z * TerrainSize + x];
                vertices[count].z = static_cast<float>(z) / (TerrainSize - 1) * TERRAIN_CHUNK_SIZE;

                normals[count] = ComputeNormal(heightMap, x + 1, z + 1);

                uvs[count].x = static_cast<float>(x) / static_cast<float>(TerrainSize - 1);
                uvs[count].y = static_cast<float>(z) / static_cast<float>(TerrainSize - 1);

                count++;
            }
        }

        count = 0;

        for (int z = 0; z < TerrainSize - 1; z++)
        {
            for (int x = 0; x < TerrainSize - 1; x++)
            {
                const auto topLeft = (z * TerrainSize) + x;
                const auto topRight = topLeft + 1;
                const auto bottomLeft = ((z + 1) * TerrainSize) + x;
                const auto bottomRight = bottomLeft + 1;

                indices[count++] = topLeft;
                indices[count++] = bottomLeft;
                indices[count++] = topRight;
                indices[count++] = topRight;
                indices[count++] = bottomLeft;
                indices[count++] = bottomRight;
            }
        }

        mesh->SetVertices(reinterpret_cast<float*>(vertices), totalVertexCount * sizeof(Vector3f));
        mesh->SetNormals(reinterpret_cast<float*>(normals), totalVertexCount * sizeof(Vector3f));
        mesh->SetUvs(reinterpret_cast<float*>(uvs), totalVertexCount * sizeof(Vector2f));
        mesh->SetIndices(indices, totalIndexCount * sizeof(std::uint32_t));

        delete[] vertices;
        delete[] indices;
        delete[] normals;
        delete[] uvs;
    }

    void GenerateTerrainChunk(TerrainChunk& chunk)
    {
        Physics3D::Terrain::Destroy(&chunk.PhysicsData);
        Physics3D::Terrain::Prepare(&chunk.PhysicsData, chunk.HeightData);

        GenerateMeshFromHeightmap<TERRAIN_CHUNK_VERTEX_COUNT>(chunk.ChunkMesh, chunk.HeightData);
        GenerateMeshFromHeightmap<TERRAIN_CHUNK_VERTEX_COUNT_LP>(chunk.ChunkMeshLowPoly, DownscaleHeightMap(chunk.HeightData));
    }
}

bool Terrain::LoadAssetData(const ByteSpan& span)
{
    TerrainSerializer terrainSerializer;

    if (!terrainSerializer.Read(span))
    {
        return false;
    }

    for (size_t i{}; i < terrainSerializer.Chunks.GetDataCount();i++)
    {
        TerrainChunkSerializer chunkSerializer;

        if (!chunkSerializer.Read(terrainSerializer.Chunks.GetData(i)))
        {
            return false;
        }

        TerrainChunk chunk;

        chunkSerializer.Position.Read(chunk.Position);
        chunkSerializer.HeightData.Read(chunk.HeightData);

        m_Chunks.push_back(chunk);
    }

    return true;
}

ByteSpan Terrain::SaveAssetData()
{
    TerrainSerializer terrainSerializer;

    for (const auto& chunk : m_Chunks)
    {
        TerrainChunkSerializer chunkSerializer;

        chunkSerializer.Position.Write(chunk.Position);
        chunkSerializer.HeightData.Write(chunk.HeightData);

        terrainSerializer.Chunks.AddData(chunkSerializer.Write());
    }

    return terrainSerializer.Write();
}

Terrain::Terrain()
{
    m_Type = AssetType::Terrain;
}

void Terrain::CreateChunk(Vector2i position)
{
    TerrainChunk chunk;

    chunk.Position = position;

    m_Chunks.push_back(chunk);
}

void Terrain::GenerateMesh()
{
    PINE_PF_SCOPE();

    PInfo(fmt::format("Generating terrain {}...", m_FilePath.string()));

    for (auto& chunk : m_Chunks)
    {
        if (chunk.ChunkMesh)
        {
            chunk.ChunkMesh->Dispose();
            delete chunk.ChunkMesh;
        }

        if (chunk.ChunkMeshLowPoly)
        {
            chunk.ChunkMeshLowPoly->Dispose();
            delete chunk.ChunkMeshLowPoly;
        }

        chunk.ChunkMesh = new Mesh(nullptr);
        chunk.ChunkMeshLowPoly = new Mesh(nullptr);

        GenerateTerrainChunk(chunk);

        chunk.IsReady = true;
    }
}

void Terrain::GenerateFromPerlinNoise(TerrainChunk& chunk, const TerrainPerlinSettings& perlinSettings)
{
    chunk.HeightData = GeneratePerlinNoiseHeightmapData(Vector2f(chunk.Position) * Vector2f(TERRAIN_CHUNK_VERTEX_COUNT - 1), perlinSettings);
}

std::vector<TerrainChunk>& Terrain::GetChunks()
{
    return m_Chunks;
}

void Terrain::Dispose()
{
    for (auto& chunk : m_Chunks)
    {
        if (chunk.ChunkMesh)
        {
            chunk.ChunkMesh->Dispose();
            delete chunk.ChunkMesh;
        }

        if (chunk.ChunkMeshLowPoly)
        {
            chunk.ChunkMeshLowPoly->Dispose();
            delete chunk.ChunkMeshLowPoly;
        }

        if (chunk.PhysicsData.PhysicsHeightField != nullptr)
        {
            Physics3D::Terrain::Destroy(&chunk.PhysicsData);
        }
    }
}

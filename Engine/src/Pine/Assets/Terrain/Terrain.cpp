#include "Terrain.hpp"

#include <stb_image.h>
#include <cooking/PxCooking.h>
#include <extensions/PxDefaultStreams.h>
#include <geometry/PxHeightFieldDesc.h>
#include <geometry/PxHeightFieldSample.h>

#include "Pine/Assets/Mesh/Mesh.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "../../Core/Serialization/Json/SerializationJson.hpp"
#include "Pine/Performance/Performance.hpp"

#include "PerlinNoise.hpp"
#include "Pine/Physics/Physics3D/Physics3D.hpp"
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

    float* GenerateHeightmapData(int size)
    {
        const siv::PerlinNoise::seed_type seed = 123456u;
        const siv::PerlinNoise perlin{ seed };

        auto buff = new float[size * size];

        for (int y = 0; y < size; y++)
        {
            for (int x = 0; x < size;x++)
            {
                buff[y * size + x] = perlin.octave2D_11(x * 0.004f, y * 0.004f, 8) * 5.f;
            }
        }

        return buff;
    }

    float GetHeight(const float* heightMap, int x, int z)
    {
        if (0 > x) x = 0;
        if (0 > z) z = 0;

        int arrayIndex = z * TERRAIN_CHUNK_VERTEX_COUNT + x;
        if (arrayIndex >= (TERRAIN_CHUNK_VERTEX_COUNT * TERRAIN_CHUNK_VERTEX_COUNT))
        {
            arrayIndex = TERRAIN_CHUNK_VERTEX_COUNT * TERRAIN_CHUNK_VERTEX_COUNT - 1;
        }

        return heightMap[arrayIndex];
    }

    Vector3f ComputeNormal(const float* heightMap, int x, int z)
    {
        float l = GetHeight(heightMap, x - 1, z);
        float r = GetHeight(heightMap, x + 1, z);
        float d = GetHeight(heightMap, x, z - 1);
        float u = GetHeight(heightMap, x, z + 1);

        return glm::normalize(Vector3f(l - r, 2.f, d - u));
    }

    void GenerateMesh(Mesh* mesh, float* heightMap)
    {
        std::uint32_t totalIndexCount = 6*(TERRAIN_CHUNK_VERTEX_COUNT - 1) * TERRAIN_CHUNK_VERTEX_COUNT;
        std::uint32_t totalVertexCount = TERRAIN_CHUNK_VERTEX_COUNT * TERRAIN_CHUNK_VERTEX_COUNT;

        auto vertices   = new Vector3f[totalVertexCount];
        auto normals    = new Vector3f[totalVertexCount];
        auto uvs        = new Vector2f[totalVertexCount];
        auto indices    = new std::uint32_t[totalIndexCount];

        std::uint32_t count = 0;

        for (int z = 0; z < TERRAIN_CHUNK_VERTEX_COUNT; z++)
        {
            for (int x = 0; x < TERRAIN_CHUNK_VERTEX_COUNT; x++)
            {
                vertices[count].x = (-static_cast<float>(x) / static_cast<float>(TERRAIN_CHUNK_VERTEX_COUNT - 1) * static_cast<float>(TERRAIN_CHUNK_SIZE)) + TERRAIN_CHUNK_SIZE / 2.f;
                vertices[count].y = heightMap[z * TERRAIN_CHUNK_VERTEX_COUNT + x];
                vertices[count].z = (-static_cast<float>(z) / static_cast<float>(TERRAIN_CHUNK_VERTEX_COUNT - 1) * static_cast<float>(TERRAIN_CHUNK_SIZE)) + TERRAIN_CHUNK_SIZE / 2.f;

                normals[count] = ComputeNormal(heightMap, x, z);

                uvs[count].x = static_cast<float>(x) / static_cast<float>(TERRAIN_CHUNK_VERTEX_COUNT - 1);
                uvs[count].y = static_cast<float>(z) / static_cast<float>(TERRAIN_CHUNK_VERTEX_COUNT - 1);

                count++;
            }
        }

        count = 0;

        for (int z = 0; z < TERRAIN_CHUNK_VERTEX_COUNT - 1; z++)
        {
            for (int x = 0; x < TERRAIN_CHUNK_VERTEX_COUNT - 1; x++)
            {
                const auto topLeft = (z * TERRAIN_CHUNK_VERTEX_COUNT) + x;
                const auto topRight = topLeft + 1;
                const auto bottomLeft = ((z + 1) * TERRAIN_CHUNK_VERTEX_COUNT) + x;
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
        /*
        if (chunk.HeightmapTexture.Get() == nullptr)
        {
            Log::Warning("Unable to generate chunk, missing heightmap texture.");
            return;
        }

        auto heightMap = GetHeightmapData(TERRAIN_CHUNK_VERTEX_COUNT, chunk.HeightmapTexture.Get());

        if (!heightMap)
        {
            Log::Warning("Unable to generate chunk, invalid heightmap texture.");
            return;
        }
        */

        auto heightMap = GenerateHeightmapData(TERRAIN_CHUNK_VERTEX_COUNT);

        GenerateMesh(chunk.ChunkMesh, heightMap);

        Physics3D::Terrain::Prepare(&chunk.PhysicsData, heightMap);

        free(heightMap);
    }
}

Pine::Terrain::Terrain()
{
    m_Type = AssetType::Terrain;
}

void Pine::Terrain::CreateChunk(Vector2i position, Pine::Texture2D* heightMap)
{
    TerrainChunk chunk;

    chunk.Position = position;
    chunk.HeightmapTexture = heightMap;

    m_Chunks.push_back(chunk);
}

void Pine::Terrain::Generate()
{
    PINE_PF_SCOPE();

    Log::Info(fmt::format("Generating terrain {}...", m_FilePath.string()));

    for (auto& chunk : m_Chunks)
    {
        chunk.ChunkMesh = new Mesh(nullptr);

        GenerateTerrainChunk(chunk);

        chunk.IsReady = true;
    }
}

std::vector<Pine::TerrainChunk>& Pine::Terrain::GetChunks()
{
    return m_Chunks;
}

/*

bool Pine::Terrain::LoadFromFile(AssetLoadStage stage)
{
    const auto json = SerializationJson::LoadFromFile(m_FilePath);

    if (!json.has_value())
    {
        return false;
    }

    const auto& j = json.value();

    for (const auto& chunkJson : j["chunks"])
    {
        Vector2f position;
        AssetHandle<Pine::Texture2D> heightmap;

        SerializationJson::LoadVector2(chunkJson, "position", position);
      //  Serialization::LoadAsset(chunkJson, "hmt", heightmap, false);

        CreateChunk(position, nullptr);
    }

    Generate();

    m_State = AssetState::Loaded;

    return true;
}

bool Pine::Terrain::SaveToFile()
{
    nlohmann::json j;

    for (const auto& chunk : m_Chunks)
    {
        nlohmann::json chunkJson;

        chunkJson["position"] = SerializationJson::StoreVector2(chunk.Position);
        chunkJson["hmt"] = SerializationJson::StoreAsset(chunk.HeightmapTexture);

        j["chunks"].push_back(chunkJson);
    }

    SerializationJson::SaveToFile(m_FilePath, j);

    return true;
}
*/

void Pine::Terrain::Dispose()
{
    for (auto& chunk : m_Chunks)
    {
        if (chunk.ChunkMesh)
        {
            chunk.ChunkMesh->Dispose();
        }

        if (chunk.PhysicsData.PhysicsHeightField != nullptr)
        {
            Physics3D::Terrain::Destroy(&chunk.PhysicsData);
        }
    }
}

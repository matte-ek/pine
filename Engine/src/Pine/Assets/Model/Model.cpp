#include "Model.hpp"
#include "Pine/Assets/Assets.hpp"

#include "Importer/ModelImporter.hpp"
#include "Pine/Threading/Threading.hpp"

namespace
{
    using namespace Pine;
}

Model::Model()
{
    m_Type = AssetType::Model;
}

bool Model::LoadAssetData(const ByteSpan& span)
{
    ModelSerializer modelSerializer;

    if (!modelSerializer.Read(span))
    {
        return false;
    }

    for (size_t i{}; i < modelSerializer.Meshes.GetDataCount();i++)
    {
        MeshSerializer meshSerializer;

        if (!meshSerializer.Read(modelSerializer.Meshes.GetData(i)))
        {
            return false;
        }

        MeshData data;

        meshSerializer.Vertices.Read(data.Vertices);
        meshSerializer.Normals.Read(data.Normals);
        meshSerializer.Tangents.Read(data.Tangents);
        meshSerializer.UVs.Read(data.UVs);
        meshSerializer.Indices.Read(data.Indices);
        meshSerializer.Material.Read(data.Material);
        meshSerializer.BoundingBoxMin.Read(data.BoundingBoxMin);
        meshSerializer.BoundingBoxMax.Read(data.BoundingBoxMax);

        m_MeshData.push_back(std::move(data));
    }

    auto task = Threading::QueueTask<void>([this]()
    {
        for (auto& meshData : m_MeshData)
        {
            auto mesh = CreateMesh();

            mesh->SetVertices(reinterpret_cast<float*>(meshData.Vertices.data()), meshData.Vertices.size() * sizeof(Vector3f));

            if (!meshData.Normals.empty())
            {
                mesh->SetNormals(reinterpret_cast<float*>(meshData.Normals.data()), meshData.Normals.size() * sizeof(Vector3f));
            }

            if (!meshData.Tangents.empty())
            {
                mesh->SetTangents(reinterpret_cast<float*>(meshData.Tangents.data()), meshData.Tangents.size() * sizeof(Vector3f));
            }

            if (!meshData.UVs.empty())
            {
                mesh->SetUvs(reinterpret_cast<float*>(meshData.UVs.data()), meshData.UVs.size() * sizeof(Vector2f));
            }

            if (!meshData.Indices.empty())
            {
                mesh->SetIndices(meshData.Indices.data(), meshData.Indices.size() * sizeof(std::uint32_t));
            }

            if (meshData.Material != UId::Empty())
            {
                mesh->SetMaterial(meshData.Material);
            }

            mesh->SetAABB(meshData.BoundingBoxMin, meshData.BoundingBoxMax);

            m_BoundingBoxMin = glm::min(meshData.BoundingBoxMin, m_BoundingBoxMin);
            m_BoundingBoxMax = glm::max(meshData.BoundingBoxMax, m_BoundingBoxMax);
        }

        // We don't need this data anymore.
        m_MeshData.clear();
    }, TaskThreadingMode::MainThread);

    Threading::AwaitTaskResult(task);

    return true;
}

ByteSpan Model::SaveAssetData()
{
    ModelSerializer modelSerializer;

    if (m_MeshData.empty())
    {
        // See Texture2D's SaveAssetData() for why.
        if (!m_FilePath.empty() && std::filesystem::exists(m_FilePath))
        {
            modelSerializer.Read(m_FilePath);
        }
    }
    else
    {
        for (const auto& meshData : m_MeshData)
        {
            MeshSerializer meshSerializer;

            meshSerializer.Vertices.Write(meshData.Vertices);
            meshSerializer.Normals.Write(meshData.Normals);
            meshSerializer.Tangents.Write(meshData.Tangents);
            meshSerializer.UVs.Write(meshData.UVs);
            meshSerializer.Indices.Write(meshData.Indices);
            meshSerializer.Material.Write(meshData.Material);
            meshSerializer.BoundingBoxMin.Write(meshData.BoundingBoxMin);
            meshSerializer.BoundingBoxMax.Write(meshData.BoundingBoxMax);

            modelSerializer.Meshes.AddData(meshSerializer.Write());
        }
    }

    return modelSerializer.Write();
}

Mesh* Model::CreateMesh()
{
    auto mesh = new Mesh(this);

    m_Meshes.push_back(mesh);

    return mesh;
}

const std::vector<Mesh*> &Model::GetMeshes() const
{
    return m_Meshes;
}

const Vector3f& Model::GetBoundingBoxMin() const
{
    return m_BoundingBoxMin;
}

const Vector3f& Model::GetBoundingBoxMax() const
{
    return m_BoundingBoxMax;
}

bool Model::Import()
{
    return Importer::ModelImporter::Import(this);
}

void Model::Dispose()
{
    m_MeshData.clear();

    for (const auto mesh: m_Meshes)
    {
        mesh->Dispose();

        delete mesh;
    }

    m_Meshes.clear();
}
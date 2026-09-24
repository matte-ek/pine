#include "Model.hpp"
#include "Pine/Assets/Assets.hpp"

#include <limits>

#include "Importer/ModelImporter.hpp"
#include "Pine/Threading/Threading.hpp"

namespace
{
    using namespace Pine;
}

bool Model::LoadAssetData(const ByteSpan& span)
{
    ModelSerializer modelSerializer;

    if (!modelSerializer.Read(span))
    {
        return false;
    }

    m_MeshData.clear();
    m_EmbeddedMaterials.clear();

    // Load embedded materials
    for (size_t i{}; i < modelSerializer.EmbeddedMaterials.GetDataCount();i++)
    {
        auto material = dynamic_cast<Material*>(Load(modelSerializer.EmbeddedMaterials.GetData(i)));

        if (!material)
        {
            continue;
        }

        Assets::Internal::RegisterAsset(material);

        // Lets the editor save an edit to the material by saving this model, which stores it.
        material->m_EmbeddingModel = m_UId;

        m_EmbeddedMaterials.push_back(material);
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

    m_LodLevels.clear();

    for (size_t i{}; i < modelSerializer.LodLevels.GetDataCount(); i++)
    {
        LodLevelSerializer lodLevelSerializer;

        if (!lodLevelSerializer.Read(modelSerializer.LodLevels.GetData(i)))
        {
            return false;
        }

        ModelLodLevel level;

        lodLevelSerializer.LodModel.Read(level.LodModel);
        lodLevelSerializer.Distance.Read(level.Distance);

        m_LodLevels.push_back(level);
    }

    m_LodCullDistance = 0.f;
    modelSerializer.LodCullDistance.Read(m_LodCullDistance);

    auto task = Threading::QueueTask<void>([this]()
    {
        // Reload replaces the GPU meshes as well as their serialized data.
        for (const auto mesh : m_Meshes)
        {
            mesh->Dispose();
            delete mesh;
        }
        m_Meshes.clear();

        // Seeded inside-out rather than at zero, so the aggregate is the union of the meshes and
        // not the union of the meshes and the origin. A model authored away from the origin would
        // otherwise report a box stretching back to it, which throws off anything that centers or
        // frames on these bounds. Reset per load, since a re-import runs this again.
        m_BoundingBoxMin = Vector3f(std::numeric_limits<float>::max());
        m_BoundingBoxMax = Vector3f(std::numeric_limits<float>::lowest());

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

        if (m_MeshData.empty())
        {
            m_BoundingBoxMin = Vector3f(0.f);
            m_BoundingBoxMax = Vector3f(0.f);
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
        // See Asset::ReadStoredAssetData() for why. This used to read the file with the
        // serializer's own path overload, which reads it raw - so it did not even get as far as
        // inflating the '.passet', let alone finding the geometry inside it.
        modelSerializer.Read(ReadStoredAssetData());

        // That read fills every field, not only the geometry this branch is after, and reading an
        // array appends to it. The fields below are written from memory, so they start empty.
        modelSerializer.EmbeddedMaterials.Reset();
        modelSerializer.LodLevels.Reset();

        // The geometry is carried forward as stored, but the material each mesh uses can be
        // changed in the editor after loading, so that part is taken from memory.
        std::vector<ByteSpan> storedMeshes;

        for (size_t i{}; i < modelSerializer.Meshes.GetDataCount(); i++)
        {
            MeshSerializer meshSerializer;

            meshSerializer.Read(modelSerializer.Meshes.GetData(i));

            if (i < m_Meshes.size())
            {
                meshSerializer.Material.Write(m_Meshes[i]->GetMaterialUId());
            }

            storedMeshes.push_back(meshSerializer.Write());
        }

        modelSerializer.Meshes.Reset();

        for (const auto& storedMesh : storedMeshes)
        {
            modelSerializer.Meshes.AddData(storedMesh);
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

    for (const auto& embeddedMaterial : m_EmbeddedMaterials)
    {
        modelSerializer.EmbeddedMaterials.AddData(embeddedMaterial->Save());
    }

    for (const auto& level : m_LodLevels)
    {
        LodLevelSerializer lodLevelSerializer;

        lodLevelSerializer.LodModel.Write(level.LodModel);
        lodLevelSerializer.Distance.Write(level.Distance);

        modelSerializer.LodLevels.AddData(lodLevelSerializer.Write());
    }

    modelSerializer.LodCullDistance.Write(m_LodCullDistance);

    return modelSerializer.Write();
}

Model::Model()
{
    m_Type = AssetType::Model;
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

void Model::SetLodLevels(const std::vector<ModelLodLevel>& levels)
{
    m_LodLevels = levels;
}

const std::vector<ModelLodLevel>& Model::GetLodLevels() const
{
    return m_LodLevels;
}

void Model::SetLodCullDistance(const float distance)
{
    m_LodCullDistance = distance;
}

float Model::GetLodCullDistance() const
{
    return m_LodCullDistance;
}

Model* Model::SelectLod(const float scaledDistance)
{
    if (m_LodCullDistance > 0.f && scaledDistance >= m_LodCullDistance)
    {
        return nullptr;
    }

    // The furthest level the object has reached. The levels are not kept sorted, so the editor can
    // show them in the order they were entered.
    Model* selectedModel = this;
    float selectedDistance = 0.f;

    for (const auto& level : m_LodLevels)
    {
        if (scaledDistance < level.Distance || level.Distance < selectedDistance)
        {
            continue;
        }

        const auto lodModel = level.LodModel.Get();

        if (lodModel == nullptr)
        {
            continue;
        }

        selectedModel = lodModel;
        selectedDistance = level.Distance;
    }

    return selectedModel;
}

bool Model::Import(Importer::AssetImport* context)
{
    return Importer::ModelImporter::Import(context, this);
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

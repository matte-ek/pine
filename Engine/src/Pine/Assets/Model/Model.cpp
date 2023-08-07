#include "Model.hpp"
#include "Pine/Core/Log/Log.hpp"
#include <fmt/format.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace
{
    using namespace Pine;

    void ProcessMesh(Model *engineModel, aiMesh *mesh, const aiScene *scene)
    {
        MeshLoadData loadData;

        loadData.VertexCount = mesh->mNumVertices;
        loadData.Vertices = static_cast<Vector3f *>(malloc(loadData.VertexCount * sizeof(Vector3f)));

        memcpy(loadData.Vertices, mesh->mVertices, loadData.VertexCount * sizeof(Vector3f));

        if (mesh->HasNormals())
        {
            loadData.Normals = static_cast<Vector3f *>(malloc(loadData.VertexCount * sizeof(Vector3f)));
            memcpy(loadData.Normals, mesh->mNormals, loadData.VertexCount * sizeof(Vector3f));
        }

        if (mesh->HasTangentsAndBitangents())
        {
            loadData.Tangents = static_cast<Vector3f *>(malloc(loadData.VertexCount * sizeof(Vector3f)));
            memcpy(loadData.Tangents, mesh->mVertices, loadData.VertexCount * sizeof(Vector3f));
        }

        if (mesh->HasTextureCoords(0))
        {
            loadData.UVs = static_cast<Vector2f *>(malloc(loadData.VertexCount * sizeof(Vector2f)));
            memcpy(loadData.UVs, mesh->mTextureCoords[0], loadData.VertexCount * sizeof(Vector2f));
        }

        std::vector<std::uint32_t> indices;

        for (unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }

        if (!indices.empty())
        {
            // This should always be a normal 4 byte integer array but w/e
            loadData.Indices = static_cast<std::uint32_t*>(malloc(sizeof(std::uint32_t) * indices.size()));
            loadData.IndicesLength = indices.size();

            memcpy(loadData.Indices, indices.data(), sizeof(std::uint32_t) * indices.size());
        }

        engineModel->AddMeshLoadData(loadData);
    }

    void ProcessNode(Model *model, aiNode *node, const aiScene *scene)
    {
        // Loop through all the meshes within the model
        for (int i = 0; i < node->mNumMeshes; i++)
        {
            const auto mesh = scene->mMeshes[node->mMeshes[i]];

            ProcessMesh(model, mesh, scene);
        }

        // Process additional nodes via the magic of recursion
        for (int i = 0; i < node->mNumChildren; i++)
        {
            ProcessNode(model, node->mChildren[i], scene);
        }
    }
}

Pine::Model::Model()
{
    m_Type = AssetType::Model;
    m_LoadMode = Pine::AssetLoadMode::MultiThreadPrepare;
}

bool Pine::Model::LoadModel()
{
    Assimp::Importer importer;

    const auto scene = importer.ReadFile(
            m_FilePath.string(), aiProcess_Triangulate | aiProcess_GenUVCoords | aiProcess_GenNormals |
                                 aiProcess_TransformUVCoords | aiProcess_FlipUVs | aiProcess_GenBoundingBoxes |
                                 aiProcess_JoinIdenticalVertices | aiProcess_CalcTangentSpace);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        Log::Error(fmt::format("Model importing error: {}", importer.GetErrorString()));
        return false;
    }

    ProcessNode(this, scene->mRootNode, scene);

    m_State = AssetState::Preparing;

    return true;
}

void Pine::Model::UploadModel()
{
    if (m_MeshLoadData.empty())
    {
        Log::Warning("No mesh data during import.");
        return;
    }

    for (const auto& loadData : m_MeshLoadData)
    {
        auto mesh = CreateMesh();

        mesh->SetVertices(reinterpret_cast<float*>(loadData.Vertices), sizeof(Vector3f) * loadData.VertexCount);

        if (loadData.Normals)
            mesh->SetNormals(reinterpret_cast<float*>(loadData.Normals), sizeof(Vector3f) * loadData.VertexCount);
        if (loadData.UVs)
            mesh->SetUvs(reinterpret_cast<float*>(loadData.UVs), sizeof(Vector2f) * loadData.VertexCount);
        if (loadData.Tangents)
            mesh->SetTangents(reinterpret_cast<float*>(loadData.Tangents), sizeof(Vector3f) * loadData.VertexCount);
        if (loadData.Indices)
            mesh->SetIndices(reinterpret_cast<unsigned int*>(loadData.Indices), sizeof(std::uint32_t) * loadData.IndicesLength);

        free(loadData.Vertices);
        free(loadData.Normals);
        free(loadData.UVs);
        free(loadData.Tangents);
        free(loadData.Indices);
    }

    m_MeshLoadData.clear();

    m_State = AssetState::Loaded;
}

Pine::Mesh *Pine::Model::CreateMesh()
{
    auto mesh = new Mesh(this);

    m_Meshes.push_back(mesh);

    return mesh;
}

const std::vector<Pine::Mesh *> &Pine::Model::GetMeshes() const
{
    return m_Meshes;
}

void Model::AddMeshLoadData(const MeshLoadData &data)
{
    m_MeshLoadData.push_back(data);
}

bool Pine::Model::LoadFromFile(Pine::AssetLoadStage stage)
{
    if (stage == AssetLoadStage::Prepare)
    {
        return LoadModel();
    }

    UploadModel();

    return true;
}

void Pine::Model::Dispose()
{
    m_MeshLoadData.clear();

    for (const auto mesh: m_Meshes)
    {
        mesh->Dispose();

        delete mesh;
    }

    m_Meshes.clear();
}
#include "ModelImporter.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

void Pine::Importer::ModelImporter::ProcessMesh(Model* model, const aiMesh* mesh, const aiScene* scene)
{
    MeshData loadData;

    loadData.Vertices.resize(mesh->mNumVertices);
    memcpy(loadData.Vertices.data(), mesh->mVertices, mesh->mNumVertices * sizeof(Vector3f));

    if (mesh->HasNormals())
    {
        loadData.Normals.resize(mesh->mNumVertices);
        memcpy(loadData.Normals.data(), mesh->mNormals, mesh->mNumVertices * sizeof(Vector3f));
    }

    if (mesh->HasTangentsAndBitangents())
    {
        loadData.Tangents.resize(mesh->mNumVertices);
        memcpy(loadData.Tangents.data(), mesh->mVertices, mesh->mNumVertices * sizeof(Vector3f));
    }

    if (mesh->HasTextureCoords(0))
    {
        loadData.UVs.resize(mesh->mNumVertices);

        for (std::uint32_t i = 0; i < mesh->mNumVertices;i++)
        {
            loadData.UVs[i] = Vector2f(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
        }
    }

    loadData.BoundingBoxMin = Vector3f(mesh->mAABB.mMin.x, mesh->mAABB.mMin.y, mesh->mAABB.mMin.z);
    loadData.BoundingBoxMax = Vector3f(mesh->mAABB.mMax.x, mesh->mAABB.mMax.y, mesh->mAABB.mMax.z);

    for (unsigned int i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
            loadData.Indices.push_back(face.mIndices[j]);
    }

    model->m_MeshData.push_back(loadData);
}

void Pine::Importer::ModelImporter::ProcessNode(Model* model, const aiNode* node, const aiScene* scene)
{
    // Loop through all the meshes within the model
    for (std::uint32_t i = 0; i < node->mNumMeshes; i++)
    {
        const auto mesh = scene->mMeshes[node->mMeshes[i]];

        ProcessMesh(model, mesh, scene);
    }

    // Process additional nodes via the magic of recursion
    for (std::uint32_t i = 0; i < node->mNumChildren; i++)
    {
        ProcessNode(model, node->mChildren[i], scene);
    }
}

bool Pine::Importer::ModelImporter::Import(Model* model)
{
    if (model->m_SourceFiles.empty() || model->m_SourceFiles.size() > 1)
    {
        Pine::Log::Warning("Ignoring Model import, too many source files.");
        return false;
    }

    const auto& file = model->m_SourceFiles.front();

    Assimp::Importer importer;

    const auto scene = importer.ReadFile(
        file.FilePath.c_str(),
        aiProcess_Triangulate           | aiProcess_FlipUVs |
        aiProcess_GenSmoothNormals      | aiProcess_GenBoundingBoxes |
        aiProcess_JoinIdenticalVertices | aiProcess_CalcTangentSpace |
        aiProcess_GlobalScale);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        Log::Error(fmt::format("Model importing error: {}", importer.GetErrorString()));
        return false;
    }

    ProcessNode(model, scene->mRootNode, scene);

    return true;
}

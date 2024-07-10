#include "Model.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Assets/Assets.hpp"
#include <fmt/format.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace
{
    using namespace Pine;
}

void Model::ProcessMesh(aiMesh *mesh, const aiScene *scene)
{
    MeshLoadData loadData;

    loadData.FacesCount = mesh->mNumFaces;
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

        memset(loadData.UVs, 0, loadData.VertexCount * sizeof(Vector2f));

        for (std::uint32_t i = 0; i < loadData.VertexCount;i++)
        {
            loadData.UVs[i] = Vector2f(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
        }
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
        loadData.Indices = static_cast<std::uint32_t *>(malloc(sizeof(std::uint32_t) * indices.size()));
        loadData.IndicesCount = static_cast<std::uint32_t>(indices.size());

        memcpy(loadData.Indices, indices.data(), sizeof(std::uint32_t) * indices.size());
    }

    if (scene->HasMaterials())
    {
        auto material = scene->mMaterials[mesh->mMaterialIndex];
        if (material && strcmp(material->GetName().C_Str(), AI_DEFAULT_MATERIAL_NAME) != 0)
        {
            aiColor3D diffuse_color(1.f, 1.f, 1.f);
            aiColor3D ambient_color(1.f, 1.f, 1.f);

            float shininess = 1.f;

            material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse_color);
            material->Get(AI_MATKEY_COLOR_AMBIENT, ambient_color);
            material->Get(AI_MATKEY_SHININESS, shininess);

            loadData.DefaultMaterial.DiffuseColor = Vector3f(diffuse_color.r, diffuse_color.g, diffuse_color.b);
            loadData.DefaultMaterial.AmbientColor = Vector3f(ambient_color.r, ambient_color.g, ambient_color.b);
            loadData.DefaultMaterial.Shininess = shininess;

            if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0)
            {
                aiString filePath;

                material->GetTexture(aiTextureType_DIFFUSE, 0, &filePath);

                loadData.DefaultMaterial.DiffuseMap = filePath.C_Str();
            }

            if (material->GetTextureCount(aiTextureType_SPECULAR) > 0)
            {
                aiString filePath;

                material->GetTexture(aiTextureType_SPECULAR, 0, &filePath);

                loadData.DefaultMaterial.SpecularMap = filePath.C_Str();
            }

            if (material->GetTextureCount(aiTextureType_NORMALS) > 0)
            {
                aiString filePath;

                material->GetTexture(aiTextureType_NORMALS, 0, &filePath);

                loadData.DefaultMaterial.NormalMap = filePath.C_Str();
            }

            loadData.HasDefaultMaterial = true;
        }
    }

    m_MeshLoadData.push_back(loadData);
}

void Model::ProcessNode(aiNode *node, const aiScene *scene)
{
    // Loop through all the meshes within the model
    for (std::uint32_t i = 0; i < node->mNumMeshes; i++)
    {
        const auto mesh = scene->mMeshes[node->mMeshes[i]];

        ProcessMesh(mesh, scene);
    }

    // Process additional nodes via the magic of recursion
    for (std::uint32_t i = 0; i < node->mNumChildren; i++)
    {
        ProcessNode(node->mChildren[i], scene);
    }
}

Model::Model()
{
    m_Type = AssetType::Model;
    m_LoadMode = AssetLoadMode::MultiThreadPrepare;
}

bool Model::LoadModel()
{
    Assimp::Importer importer;

    const auto scene = importer.ReadFile(
            m_FilePath.string().c_str(),
                          aiProcess_Triangulate | aiProcess_GenUVCoords |
                                 aiProcess_GenNormals | aiProcess_GenBoundingBoxes |
                                 aiProcess_JoinIdenticalVertices | aiProcess_CalcTangentSpace |
                                 aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph |
                                 aiProcess_GlobalScale);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        Log::Error(fmt::format("Model importing error: {}", importer.GetErrorString()));
        return false;
    }

    ProcessNode(scene->mRootNode, scene);

    m_State = AssetState::Preparing;

    return true;
}

void Model::UploadModel()
{
    if (m_MeshLoadData.empty())
    {
        Log::Warning("No mesh data during import.");
        return;
    }

    for (int i = 0; i < m_MeshLoadData.size();i++)
    {
        const auto& loadData = m_MeshLoadData[i];
        auto mesh = CreateMesh();

        mesh->SetVertices(reinterpret_cast<float *>(loadData.Vertices), sizeof(Vector3f) * loadData.VertexCount);

        if (loadData.Normals)
            mesh->SetNormals(reinterpret_cast<float *>(loadData.Normals), sizeof(Vector3f) * loadData.VertexCount);
        if (loadData.UVs)
            mesh->SetUvs(reinterpret_cast<float *>(loadData.UVs), sizeof(Vector2f) * loadData.VertexCount);
        if (loadData.Tangents)
            mesh->SetTangents(reinterpret_cast<float *>(loadData.Tangents), sizeof(Vector3f) * loadData.VertexCount);
        if (loadData.Indices)
            mesh->SetIndices(reinterpret_cast<unsigned int *>(loadData.Indices),
                             sizeof(std::uint32_t) * loadData.IndicesCount);

        bool hasEngineMaterialKey = m_Metadata.contains("material") && m_Metadata["material"].contains(std::to_string(i));
        std::string engineMaterial = hasEngineMaterialKey ? m_Metadata["material"][std::to_string(i)] : "null";

        if (engineMaterial != "null")
        {
            mesh->SetMaterial(engineMaterial);
        }
        else
        {
            // If this mesh had any default material stored alongside it, we will create a new
            // Pine material with those properties.
            if (loadData.HasDefaultMaterial)
            {
                auto parentDirectory = m_FilePath.parent_path().string() + "/";
                auto modelName = m_FilePath.stem().string();

                auto material = new Pine::Material;

                material->SetPath(std::filesystem::path(m_Path).parent_path().string() + "/" + modelName + std::to_string(i) + ".mat");
                material->SetFilePath(parentDirectory + modelName + std::to_string(i) + ".mat");

                material->SetDiffuseColor(loadData.DefaultMaterial.DiffuseColor);
                material->SetAmbientColor(loadData.DefaultMaterial.AmbientColor);
                material->SetShininess(loadData.DefaultMaterial.Shininess);

                if (!loadData.DefaultMaterial.DiffuseMap.empty())
                    material->SetDiffuse(parentDirectory + loadData.DefaultMaterial.DiffuseMap);
                if (!loadData.DefaultMaterial.SpecularMap.empty())
                    material->SetSpecular(parentDirectory + loadData.DefaultMaterial.SpecularMap);
                if (!loadData.DefaultMaterial.NormalMap.empty())
                    material->SetNormal(parentDirectory + loadData.DefaultMaterial.NormalMap);

                mesh->SetMaterial(material);
            }
        }

        free(loadData.Vertices);
        free(loadData.Normals);
        free(loadData.UVs);
        free(loadData.Tangents);
        free(loadData.Indices);
    }

    m_MeshLoadData.clear();

    m_State = AssetState::Loaded;
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

bool Model::LoadFromFile(AssetLoadStage stage)
{
    if (stage == AssetLoadStage::Prepare)
    {
        return LoadModel();
    }

    UploadModel();

    return true;
}

void Model::Dispose()
{
    m_MeshLoadData.clear();

    for (const auto mesh: m_Meshes)
    {
        mesh->Dispose();

        delete mesh;
    }

    m_Meshes.clear();
}

bool Model::SaveToFile()
{
    for (int i = 0; i < m_Meshes.size();i++)
    {
        auto mesh = m_Meshes[i];

        if (!mesh->GetMaterial())
            continue;

        if (!mesh->GetMaterial()->IsMeshGenerated())
        {
            m_HasMetadata = true;
            m_Metadata["material"][std::to_string(i)] = mesh->GetMaterial()->GetPath();
        }
    }

    return true;
}

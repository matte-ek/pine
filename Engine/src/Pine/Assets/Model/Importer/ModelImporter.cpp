#include "ModelImporter.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "Pine/Assets/Assets.hpp"

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

    if (scene->HasMaterials())
    {
        size_t embeddedId = 0;

        for (size_t i{}; i < mesh->mMaterialIndex;i++)
        {
            auto material = scene->mMaterials[i];

            if (strcmp(material->GetName().C_Str(), AI_DEFAULT_MATERIAL_NAME) == 0)
            {
                continue;
            }

            embeddedId++;
        }

        loadData.Material = model->m_EmbeddedMaterials[embeddedId]->GetUId();
    }

    // TODO: Fixa så embedded materials sparas o laddas in till asset manager systemet
    // TODO: samt att materialen sedan laddas som de ska här ifrån :-)

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

Pine::Texture2D* Pine::Importer::ModelImporter::ImportTexture(AssetImport* context, aiMaterial* material, int type)
{
    const auto textureType = static_cast<aiTextureType>(type);

    if (material->GetTextureCount(textureType) <= 0)
    {
        return nullptr;
    }

    aiString filePath;

    material->GetTexture(textureType, 0, &filePath);

    return dynamic_cast<Texture2D*>(ImportRelative(context, filePath.C_Str()));
}

bool Pine::Importer::ModelImporter::Import(AssetImport* importContext, Model* model)
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

    if (scene->HasMaterials())
    {
        auto materialLoadRelPath = std::filesystem::path(file.FilePath).parent_path().string();

        for (size_t i{}; i < scene->mNumMaterials;i++)
        {
            auto material = scene->mMaterials[i];

            if (strcmp(material->GetName().C_Str(), AI_DEFAULT_MATERIAL_NAME) == 0)
            {
                continue;
            }

            auto engineMaterial = dynamic_cast<Material*>(Assets::CreateAsset(AssetType::Material, model->GetPath() + "#mat" + std::to_string(i)));

            aiColor3D diffuse_color(1.f, 1.f, 1.f);
            aiColor3D ambient_color(1.f, 1.f, 1.f);

            float shininess = 1.f;

            material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse_color);
            material->Get(AI_MATKEY_COLOR_AMBIENT, ambient_color);
            material->Get(AI_MATKEY_SHININESS, shininess);

            engineMaterial->SetDiffuseColor(Vector3f(diffuse_color.r, diffuse_color.g, diffuse_color.b));
            engineMaterial->SetAmbientColor(Vector3f(ambient_color.r, ambient_color.g, ambient_color.b));
            engineMaterial->SetShininess(shininess);

            if (auto diffuseTexture = ImportTexture(importContext, material, aiTextureType_DIFFUSE))
            {
                engineMaterial->SetDiffuse(diffuseTexture);
            }

            if (auto specularTexture = ImportTexture(importContext, material, aiTextureType_SPECULAR))
            {
                engineMaterial->SetSpecular(specularTexture);
            }

            if (auto normalMapTexture = ImportTexture(importContext, material, aiTextureType_NORMALS))
            {
                engineMaterial->SetNormal(normalMapTexture);
            }

            model->m_EmbeddedMaterials.push_back(engineMaterial);
        }
    }

    ProcessNode(model, scene->mRootNode, scene);

    return true;
}

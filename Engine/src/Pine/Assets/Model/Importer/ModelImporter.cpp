#include "ModelImporter.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <optional>
#include <unordered_set>

#include "Pine/Assets/Assets.hpp"
#include "Pine/Core/File/File.hpp"
#include "Pine/Core/String/String.hpp"

namespace
{
    void RemoveNonASCII(std::string& str)
    {
        str.erase(
            std::remove_if(str.begin(), str.end(),
                [](const unsigned char c)
                {
                    return c > 127;
                }),
                str.end());
    }

    // The lower-case extension of the file the model is being imported from, e.g. ".obj".
    std::string ModelExtension(const Pine::Importer::AssetImport* context)
    {
        if (context->SourcePaths.empty())
        {
            return {};
        }

        return Pine::String::ToLower(context->SourcePaths.front().extension().string());
    }

    // The model file tells us what each of its textures is for, which decides both the block
    // compression format and whether the texture gets sRGB-decoded when sampled. That's a much
    // better source than guessing from the file name, so use it while we still have it.
    //
    // What a slot means is partly a property of the model format, not just of assimp's enum:
    // assimp maps an OBJ's 'map_Bump' onto aiTextureType_HEIGHT rather than NORMALS, and in that
    // format that is what a normal map looks like. Elsewhere HEIGHT may be a real displacement
    // map, so it is left alone.
    //
    // 'modelExtension' is the lower-case extension of the model file, as ModelExtension() gives it.
    std::optional<Pine::TextureUsageHint> UsageHintForTextureType(
        const aiTextureType textureType,
        const std::string& modelExtension)
    {
        switch (textureType)
        {
            case aiTextureType_NORMALS:
                return Pine::TextureUsageHint::NormalMap;

            case aiTextureType_HEIGHT:
                if (modelExtension == ".obj")
                {
                    return Pine::TextureUsageHint::NormalMap;
                }

                return {};

            // Everything below carries data in colour channels: sampling it sRGB-decoded makes the
            // values it holds plain wrong.
            case aiTextureType_SPECULAR:
            case aiTextureType_METALNESS:
            case aiTextureType_DIFFUSE_ROUGHNESS:
            case aiTextureType_AMBIENT_OCCLUSION:
            case aiTextureType_LIGHTMAP:
                return Pine::TextureUsageHint::LinearColor;

            default:
                return {};
        }
    }

    bool IsSupportedImageExtension(const std::string& ext)
    {
        static const std::unordered_set<std::string> allowedExtensions = {
            "png",
            "jpg",
            "jpeg"
        };

        return allowedExtensions.count(ext) != 0;
    }

    // What an embedded texture is imported as. This ends up in the texture's asset path, so it has
    // to be usable as a file name, and it has to come out the same every time the same model is
    // imported - anything else would leave a second texture asset beside the first on a re-import.
    //
    // The name the model file carries is used when there is one, but it is optional (glTF only has
    // one when the image was given a name) and it is an arbitrary string out of the file, which
    // may well be a path from whichever machine authored the model, so only the file name part of
    // it is taken. Failing that, the texture's position in the file names it - prefixed with the model,
    // since every model in a directory imports its textures into the same place.
    std::string EmbeddedTextureName(const aiScene* scene, const aiTexture* texture, const std::string& modelName)
    {
        std::string fileName = texture->mFilename.C_Str();

        RemoveNonASCII(fileName);

        if (const auto directoryEnd = fileName.find_last_of("/\\"); directoryEnd != std::string::npos)
        {
            fileName = fileName.substr(directoryEnd + 1);
        }

        if (!fileName.empty())
        {
            return fileName;
        }

        std::size_t textureIndex = 0;

        for (unsigned int i = 0; i < scene->mNumTextures; i++)
        {
            if (scene->mTextures[i] == texture)
            {
                textureIndex = i;
                break;
            }
        }

        return fmt::format("{}-texture-{}", modelName, textureIndex);
    }
}

void Pine::Importer::ModelImporter::ProcessMesh(Model* model, const aiMesh* mesh, const std::vector<UId>& materialIds)
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
        memcpy(loadData.Tangents.data(), mesh->mTangents, mesh->mNumVertices * sizeof(Vector3f));
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

    // A mesh whose material wasn't imported, assimp's default one in particular, is left without
    // one, which leaves it on the engine's default material (see Mesh::m_Material).
    if (mesh->mMaterialIndex < materialIds.size())
    {
        loadData.Material = materialIds[mesh->mMaterialIndex];
    }

    model->m_MeshData.push_back(loadData);
}

void Pine::Importer::ModelImporter::ProcessNode(Model* model, const aiNode* node, const aiScene* scene, const std::vector<UId>& materialIds)
{
    // Loop through all the meshes within the model
    for (std::uint32_t i = 0; i < node->mNumMeshes; i++)
    {
        const auto mesh = scene->mMeshes[node->mMeshes[i]];

        ProcessMesh(model, mesh, materialIds);
    }

    // Process additional nodes via the magic of recursion
    for (std::uint32_t i = 0; i < node->mNumChildren; i++)
    {
        ProcessNode(model, node->mChildren[i], scene, materialIds);
    }
}

Pine::Texture2D* Pine::Importer::ModelImporter::ImportTexture(AssetImport* context, const aiScene* scene, aiMaterial* material, int type)
{
    const auto textureType = static_cast<aiTextureType>(type);

    if (material->GetTextureCount(textureType) <= 0)
    {
        return nullptr;
    }

    std::function<bool(Asset*)> configure = nullptr;

    if (const auto usageHint = UsageHintForTextureType(textureType, ModelExtension(context)))
    {
        configure = [usageHint](Asset* asset)
        {
            if (const auto texture = dynamic_cast<Texture2D*>(asset))
            {
                const auto previous = texture->GetImportConfiguration();
                ApplyTextureUsageHint(
                    texture->GetImportConfiguration(), *usageHint, TextureUsageHintSource::SourceFormat);
                const auto& current = texture->GetImportConfiguration();
                return current.UsageHint != previous.UsageHint ||
                       current.UsageHintSource != previous.UsageHintSource;
            }
            return false;
        };
    }

    aiString filePath;
    material->GetTexture(textureType, 0, &filePath);

    const auto embeddedTexture = scene->GetEmbeddedTexture(filePath.C_Str());

    if (embeddedTexture == nullptr)
    {
        return dynamic_cast<Texture2D*>(ImportRelative(context, filePath.C_Str(), "", configure));
    }

    // If the image height is 0, then the texture is embedded as a image file
    // e.g. PNG or JPEG. Otherwise, it's stored as a raw texture, which nothing here decodes.
    if (embeddedTexture->mHeight != 0)
    {
        PWarning(fmt::format("Ignoring raw embedded texture '{}' in model, only image files are supported.",
            filePath.C_Str()));
        return nullptr;
    }

    if (!IsSupportedImageExtension(embeddedTexture->achFormatHint))
    {
        PWarning(fmt::format("Unsupported image format in model: {}", embeddedTexture->achFormatHint));
        return nullptr;
    }

    if (!std::filesystem::exists("import-cache"))
    {
        std::filesystem::create_directory("import-cache");
    }

    // No way we're even touching arbitrary data to a new path with a arbitrary string,
    // temporarily use a UID instead so I can sleep better at night. What the texture is imported
    // as is a separate matter, see EmbeddedTextureName().
    const auto temporaryFilePath = fmt::format("import-cache/{}.{}", UId::New().ToString(), embeddedTexture->achFormatHint);

    const auto imageBytes = ByteSpan(reinterpret_cast<const std::byte*>(embeddedTexture->pcData), embeddedTexture->mWidth);

    File::WriteRaw(temporaryFilePath, imageBytes);

    if (!std::filesystem::exists(temporaryFilePath))
    {
        PError(fmt::format("Failed to write embedded texture to {}", temporaryFilePath));
        return nullptr;
    }

    const auto textureName = EmbeddedTextureName(scene, embeddedTexture, std::filesystem::path(context->EnginePath).stem().string());

    const auto texture = dynamic_cast<Texture2D*>(ImportRelative(context, temporaryFilePath, textureName, configure));

    std::filesystem::remove(temporaryFilePath);

    return texture;
}

bool Pine::Importer::ModelImporter::Import(AssetImport* importContext, Model* model)
{
    if (model->m_SourceFiles.size() != 1)
    {
        PWarning(fmt::format("Ignoring Model import, expected exactly one source file, got {}.",
            model->m_SourceFiles.size()));
        return false;
    }

    const auto& file = model->m_SourceFiles.front();

    Assimp::Importer importer;

    // Only meshes that ship without normals get generated ones (no aiProcess_ForceGenNormals),
    // so authored hard edges survive. For that fallback, don't smooth across sharp edges:
    // Assimp's default is 175 degrees, which averages e.g. a wall face with its edge bevel
    // and bends the normals along the edges of otherwise flat surfaces.
    importer.SetPropertyFloat(AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, 80.f);

    const auto scene = importer.ReadFile(
        file.FilePath.c_str(),
        aiProcess_Triangulate           | aiProcess_FlipUVs |
        aiProcess_GenSmoothNormals      | aiProcess_GenBoundingBoxes |
        aiProcess_CalcTangentSpace      | aiProcess_GlobalScale |
        aiProcess_PreTransformVertices  | aiProcess_FindInvalidData);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        PError(fmt::format("Model importing error: {}", importer.GetErrorString()));
        return false;
    }

    // Importing appends, so a re-import has to start from a clean model. The embedded materials
    // are kept aside rather than dropped: meshes reference their material by UId, so a material
    // that is still in the file must come back as the same asset, not a new one.
    auto previousMaterials = std::move(model->m_EmbeddedMaterials);

    model->m_EmbeddedMaterials.clear();
    model->m_MeshData.clear();

    // Resolved while the materials are imported, so the meshes don't have to work out again which
    // of the file's materials were skipped. Entries stay empty for the skipped ones.
    std::vector<UId> materialIds;

    const auto modelExtension = ModelExtension(importContext);

    if (scene->HasMaterials())
    {
        materialIds.resize(scene->mNumMaterials, UId::Empty());

        // Assimp adds a material of its own for meshes that don't name one, and not every format
        // gives it a name we can recognise - glTF's is simply unnamed, so the check below misses
        // it and it used to be imported as an empty-named material of its own. Going by what the
        // meshes actually reference catches it whatever it is called, and leaves out any other
        // material the file carries but never uses.
        std::vector<bool> isMaterialUsed(scene->mNumMaterials, false);

        for (std::uint32_t i = 0; i < scene->mNumMeshes; i++)
        {
            const auto materialIndex = scene->mMeshes[i]->mMaterialIndex;

            if (materialIndex < isMaterialUsed.size())
            {
                isMaterialUsed[materialIndex] = true;
            }
        }

        for (size_t i{}; i < scene->mNumMaterials;i++)
        {
            if (!isMaterialUsed[i])
            {
                continue;
            }

            auto material = scene->mMaterials[i];

            // Assimp hands out a default material for meshes that don't name one. There's nothing
            // in it worth importing, and the engine already has a default of its own.
            if (strcmp(material->GetName().C_Str(), AI_DEFAULT_MATERIAL_NAME) == 0)
            {
                continue;
            }

            auto materialName = std::string(material->GetName().C_Str());

            RemoveNonASCII(materialName);

            // A material doesn't have to be named - glTF's names are optional - and the name is
            // the only thing telling one model's materials apart, so fall back to the material's
            // position in the file rather than importing several of them to the same path.
            if (materialName.empty())
            {
                materialName = fmt::format("material-{}", i);
            }

            const auto materialPath = String::ToLower(model->GetPath() + "-" + materialName);

            Material* engineMaterial = nullptr;

            for (const auto previousMaterial : previousMaterials)
            {
                if (previousMaterial->GetPath() == materialPath)
                {
                    engineMaterial = previousMaterial;
                    break;
                }
            }

            if (!engineMaterial)
            {
                engineMaterial = dynamic_cast<Material*>(Assets::CreateAsset(AssetType::Material, materialPath));
            }

            aiColor3D diffuse_color(1.f, 1.f, 1.f);
            aiColor3D ambient_color(0.f, 0.f, 0.f);

            float shininess = 1.f;

            material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse_color);
            material->Get(AI_MATKEY_COLOR_AMBIENT, ambient_color);
            material->Get(AI_MATKEY_SHININESS, shininess);

            engineMaterial->SetDiffuseColor(Vector3f(diffuse_color.r, diffuse_color.g, diffuse_color.b));
            engineMaterial->SetAmbientColor(Vector3f(ambient_color.r, ambient_color.g, ambient_color.b));
            engineMaterial->SetShininess(shininess);

            engineMaterial->SetDiffuse(ImportTexture(importContext, scene, material, aiTextureType_DIFFUSE));
            engineMaterial->SetSpecular(ImportTexture(importContext, scene, material, aiTextureType_SPECULAR));

            auto normalMapTexture = ImportTexture(importContext, scene, material, aiTextureType_NORMALS);

            // In a format where the height slot is where the normal map lives (an OBJ's
            // 'map_Bump'), fall back to it when the material has no normals slot of its own.
            if (!normalMapTexture &&
                UsageHintForTextureType(aiTextureType_HEIGHT, modelExtension) == TextureUsageHint::NormalMap)
            {
                normalMapTexture = ImportTexture(importContext, scene, material, aiTextureType_HEIGHT);
            }

            engineMaterial->SetNormal(normalMapTexture);

            // The diffuse texture has been imported by now, so its alpha has been measured. A
            // material a model file generates cannot be edited in the editor, so this is the only
            // chance it gets to end up in the right pass.
            engineMaterial->ResolveRenderingModeFromDiffuse();

            model->m_EmbeddedMaterials.push_back(engineMaterial);

            materialIds[i] = engineMaterial->GetUId();
        }
    }

    ProcessNode(model, scene->mRootNode, scene, materialIds);

    return true;
}

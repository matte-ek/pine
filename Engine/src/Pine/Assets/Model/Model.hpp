#pragma once

#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/Assets/Mesh/Mesh.hpp"

class aiMesh;
struct aiScene;
class aiNode;

namespace Pine
{
    struct MeshMaterialData
    {
        Pine::Vector3f DiffuseColor;
        Pine::Vector3f AmbientColor;

        float Shininess = 1.f;

        std::string DiffuseMap;
        std::string SpecularMap;
        std::string NormalMap;
    };

    struct MeshLoadData
    {
        Vector3f* Vertices = nullptr;
        Vector3f* Normals = nullptr;
        Vector3f* Tangents = nullptr;
        Vector2f* UVs = nullptr;

        std::uint32_t* Indices = nullptr;

        std::uint32_t VertexCount = 0;
        std::uint32_t FacesCount = 0;
        std::uint32_t IndicesCount = 0;

        bool HasDefaultMaterial = false;
        MeshMaterialData DefaultMaterial;
    };

    class Model : public IAsset
    {
    protected:
        std::vector<Mesh*> m_Meshes;
        std::vector<MeshLoadData> m_MeshLoadData;

        bool m_UsedAsCollider = false;

        void ProcessMesh(aiMesh *mesh, const aiScene *scene);
        void ProcessNode(const aiNode *node, const aiScene *scene);

        bool LoadModel();
        void UploadModel();
    public:
        Model();

        Mesh* CreateMesh();
        const std::vector<Mesh*>& GetMeshes() const;

        bool LoadFromFile(AssetLoadStage stage) override;
        bool SaveToFile() override;

        void Dispose() override;

        friend class Collider;
    };
}
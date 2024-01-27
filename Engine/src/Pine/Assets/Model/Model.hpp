#pragma once

#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/Assets/Mesh/Mesh.hpp"

namespace Pine
{
    struct MeshLoadData
    {
        Pine::Material* Material = nullptr;

        Vector3f* Vertices = nullptr;
        Vector3f* Normals = nullptr;
        Vector3f* Tangents = nullptr;
        Vector2f* UVs = nullptr;

        std::uint32_t VertexCount = 0;

        std::uint32_t* Indices = nullptr;
        std::uint32_t IndicesLength = 0;

        std::uint32_t Faces = 0;

        std::string DiffuseMap;
        std::string SpecularMap;
        std::string NormalMap;
    };

    class Model : public IAsset
    {
    protected:
        std::vector<Mesh*> m_Meshes;
        std::vector<MeshLoadData> m_MeshLoadData;

        bool m_UsedAsCollider = false;

        bool LoadModel();
        void UploadModel();
    public:
        Model();

        Mesh* CreateMesh();
        const std::vector<Mesh*>& GetMeshes() const;

        // TODO: Move this out of here somehow.
        void AddMeshLoadData(const MeshLoadData& data);

        bool LoadFromFile(AssetLoadStage stage) override;

        void Dispose() override;

        friend class Collider;
    };
}
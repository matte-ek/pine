#pragma once

#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Assets/Mesh/Mesh.hpp"

namespace Pine
{
    namespace Importer
    {
        class ModelImporter;
    }

    struct MeshMaterialData
    {
        Vector3f DiffuseColor;
        Vector3f AmbientColor;

        float Shininess = 1.f;

        std::string DiffuseMap;
        std::string SpecularMap;
        std::string NormalMap;
    };

    struct MeshData
    {
        std::vector<Vector3f> Vertices;
        std::vector<Vector3f> Normals;
        std::vector<Vector3f> Tangents;
        std::vector<Vector2f> UVs;
        std::vector<std::uint32_t> Indices;

        Vector3f BoundingBoxMin = {};
        Vector3f BoundingBoxMax = {};

        UId Material;
    };

    class Model : public Asset
    {
    protected:
        std::vector<Mesh*> m_Meshes;
        std::vector<MeshData> m_MeshData;

        Vector3f m_BoundingBoxMin = {};
        Vector3f m_BoundingBoxMax = {};

        bool m_UsedAsCollider = false;

        bool LoadAssetData(const ByteSpan& span) override;
        ByteSpan SaveAssetData() override;

        struct MeshSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_ARRAY_FIXED(Vertices, Vector3f);
            PINE_SERIALIZE_ARRAY_FIXED(Normals, Vector3f);
            PINE_SERIALIZE_ARRAY_FIXED(Tangents, Vector3f);
            PINE_SERIALIZE_ARRAY_FIXED(UVs, Vector2f);
            PINE_SERIALIZE_ARRAY_FIXED(Indices, std::uint32_t);

            PINE_SERIALIZE_PRIMITIVE(Material, Serialization::DataType::UId);
            
            PINE_SERIALIZE_PRIMITIVE(BoundingBoxMin, Serialization::DataType::Vec3);
            PINE_SERIALIZE_PRIMITIVE(BoundingBoxMax, Serialization::DataType::Vec3);
        };

        struct ModelSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_ARRAY(Meshes);
        };
    public:
        Model();

        Mesh* CreateMesh();

        const std::vector<Mesh*>& GetMeshes() const;

        const Vector3f& GetBoundingBoxMin() const;
        const Vector3f& GetBoundingBoxMax() const;

        bool Import() override;

        void Dispose() override;

        friend class Collider;
        friend class Importer::ModelImporter;
    };
}
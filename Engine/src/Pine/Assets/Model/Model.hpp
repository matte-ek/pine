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

    class Model;

    // One step of a model's level-of-detail chain: the model drawn from 'Distance' onwards.
    struct ModelLodLevel
    {
        AssetHandle<Model> LodModel;

        // In world units from the LOD reference camera, for an object at scale 1. Larger objects
        // switch proportionally later - see Model::SelectLod.
        float Distance = 0.f;
    };

    class Model : public Asset
    {
    protected:
        std::vector<Mesh*> m_Meshes;
        std::vector<MeshData> m_MeshData;

        std::vector<Texture2D*> m_EmbeddedTextures;
        std::vector<Material*> m_EmbeddedMaterials;

        Vector3f m_BoundingBoxMin = {};
        Vector3f m_BoundingBoxMax = {};

        bool m_UsedAsCollider = false;

        // Only read from the model a ModelRenderer points at: a model used as one of these levels
        // is drawn as it is, whatever LOD levels it has of its own.
        std::vector<ModelLodLevel> m_LodLevels;

        // Past this distance, measured like ModelLodLevel::Distance, the object is not drawn at
        // all. 0 draws it at any distance.
        float m_LodCullDistance = 0.f;

        bool LoadAssetData(const ByteSpan& span) override;
        ByteSpan SaveAssetData() override;

        struct MeshSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_ARRAY_FIXED(Vertices, Vector3f);
            PINE_SERIALIZE_ARRAY_FIXED(Normals, Vector3f);
            PINE_SERIALIZE_ARRAY_FIXED(Tangents, Vector3f);
            PINE_SERIALIZE_ARRAY_FIXED(UVs, Vector2f);
            PINE_SERIALIZE_ARRAY_FIXED(Indices, std::uint32_t);

            PINE_SERIALIZE_PRIMITIVE(BoundingBoxMin, Serialization::DataType::Vec3);
            PINE_SERIALIZE_PRIMITIVE(BoundingBoxMax, Serialization::DataType::Vec3);

            PINE_SERIALIZE_PRIMITIVE(Material, Serialization::DataType::UId);
        };

        struct LodLevelSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_ASSET(LodModel);
            PINE_SERIALIZE_PRIMITIVE(Distance, Serialization::DataType::Float32);
        };

        struct ModelSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_ARRAY(EmbeddedMaterials);
            PINE_SERIALIZE_ARRAY(Meshes);

            PINE_SERIALIZE_ARRAY(LodLevels);
            PINE_SERIALIZE_PRIMITIVE(LodCullDistance, Serialization::DataType::Float32);
        };
    public:
        Model();

        Mesh* CreateMesh();

        const std::vector<Mesh*>& GetMeshes() const;

        const Vector3f& GetBoundingBoxMin() const;
        const Vector3f& GetBoundingBoxMax() const;

        void SetLodLevels(const std::vector<ModelLodLevel>& levels);
        const std::vector<ModelLodLevel>& GetLodLevels() const;

        void SetLodCullDistance(float distance);
        float GetLodCullDistance() const;

        // The model to draw for an object 'scaledDistance' away from the LOD reference camera,
        // already divided by the object's scale: this model, one of its LOD levels, or nullptr past
        // the cull distance. A level whose model is missing is skipped, so the nearer one stays.
        Model* SelectLod(float scaledDistance);

        bool Import(Importer::AssetImport* context) override;

        void Dispose() override;

        friend class Collider;
        friend class Importer::ModelImporter;
    };
}
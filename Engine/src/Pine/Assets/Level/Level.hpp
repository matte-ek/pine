#pragma once

#include "Pine/Assets/Blueprint/Blueprint.hpp"
#include "Pine/Assets/Asset/Asset.hpp"

namespace Pine
{
    class Texture3D;

    struct LevelSettings
    {
        AssetHandle<Texture3D> Skybox;

        Vector3f AmbientColor = Vector3f(0.05f, 0.05f, 0.05f);

        Vector4f FogColor = Vector4f(0.0f, 0.0f, 0.0f, 1.0f);
        float FogDistance = 30.f;
        float FogIntensity = 0.f;

        bool HasCamera = false;
        std::uint32_t CameraEntity = 0;
    };

    class Level : public Asset
    {
    private:
        std::vector<Blueprint*> m_Blueprints;

        LevelSettings m_LevelSettings;

        struct LevelSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_ARRAY(Blueprints);

            PINE_SERIALIZE_ASSET(Skybox);
            PINE_SERIALIZE_PRIMITIVE(AmbientColor, Serialization::DataType::Vec3);
            PINE_SERIALIZE_PRIMITIVE(FogColor, Serialization::DataType::Vec4);
            PINE_SERIALIZE_PRIMITIVE(FogDistance, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(FogIntensity, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(Camera, Serialization::DataType::Int32);
        };
    public:
        explicit Level();

        void CreateFromWorld();
        void Load();

        void ClearBlueprints();

        std::size_t GetBlueprintCount() const;

        LevelSettings& GetLevelSettings();

        bool LoadFromFile(AssetLoadStage stage) override;
        bool SaveToFile() override;

        void Dispose() override;
    };

}
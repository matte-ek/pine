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

        // HDR exposure multiplier, applied before tone mapping in the post-process pass.
        // 1.0 = neutral; higher brightens the scene before it's tone-mapped to display range.
        float Exposure = 1.0f;

        // Bloom: HDR brightness above BloomThreshold is extracted, blurred and added back, so bright
        // areas glow. BloomIntensity scales how strongly the glow is composited (0 = off).
        float BloomThreshold = 1.0f;
        float BloomIntensity = 0.6f;

        // Post-processing film look (applied in the post-process pass).
        float GrainStrength = 0.08f;
        float VignetteStrength = 0.5f;

        bool HasCamera = false;
        std::uint32_t CameraEntity = 0;
    };

    class Level : public Asset
    {
    private:
        std::vector<Blueprint*> m_Blueprints;

        LevelSettings m_LevelSettings;
        bool m_CameraUsesSerializedOrder = true;

        bool LoadAssetData(const ByteSpan& span) override;
        ByteSpan SaveAssetData() override;

        struct LevelSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_ARRAY(Blueprints);

            PINE_SERIALIZE_ASSET(Skybox);
            PINE_SERIALIZE_PRIMITIVE(AmbientColor, Serialization::DataType::Vec3);
            PINE_SERIALIZE_PRIMITIVE(FogColor, Serialization::DataType::Vec4);
            PINE_SERIALIZE_PRIMITIVE(FogDistance, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(FogIntensity, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(Exposure, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(BloomThreshold, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(BloomIntensity, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(GrainStrength, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(VignetteStrength, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(Camera, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(CameraUsesSerializedOrder, Serialization::DataType::Boolean);
        };
    public:
        explicit Level();

        void CreateFromWorld();
        void Load();

        void ClearBlueprints();

        std::size_t GetBlueprintCount() const;

        LevelSettings& GetLevelSettings();

        void Dispose() override;
    };

}

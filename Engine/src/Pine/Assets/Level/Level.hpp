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

        // Exponential height fog. FogDensity is how much fog there is per world unit at FogHeight
        // (0 = no fog), and the fog thins by a factor of e every 1 / FogHeightFalloff units above
        // it, so a ray aimed at the sky leaves most of it behind. A falloff of 0 is the same fog at
        // every height. Looking level from FogHeight, fog hides 95% of what lies 3 / FogDensity
        // units away.
        Vector4f FogColor = Vector4f(0.0f, 0.0f, 0.0f, 1.0f);
        float FogDensity = 0.f;
        float FogHeight = 0.f;
        float FogHeightFalloff = 0.1f;

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

        // Wind that sways terrain detail. WindDirection is the way it blows, as a yaw in degrees:
        // 0 blows towards +x, 90 towards +z. WindStrength is how far a tip leans at most, as a
        // share of its height above the ground (0 = still), and WindSpeed is how many gusts pass a
        // point each second.
        float WindDirection = 0.f;
        float WindStrength = 0.1f;
        float WindSpeed = 0.5f;

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
            PINE_SERIALIZE_PRIMITIVE(FogDensity, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(FogHeight, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(FogHeightFalloff, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(Exposure, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(BloomThreshold, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(BloomIntensity, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(GrainStrength, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(VignetteStrength, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(WindDirection, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(WindStrength, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(WindSpeed, Serialization::DataType::Float32);
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

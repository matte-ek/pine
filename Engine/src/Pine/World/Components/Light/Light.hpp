#pragma once
#include "Pine/World/Components/Component/Component.hpp"
#include "Pine/Core/Math/Math.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"

namespace Pine
{

    enum class LightType
    {
        Directional,
        PointLight,
        SpotLight
    };

    namespace Renderer3D
    {
        struct LightHintData
        {
            std::uint16_t LightIndex {};
        };
    }

    class Light final  : public Component
    {
    private:
        LightType m_LightType = LightType::Directional;

        Vector3f m_LightColor = Vector3f(1.f, 1.f, 1.f);

        Vector3f m_LightAttenuation = Vector3f(1.f, 0.045f, 0.0075f);

        float m_SpotlightRadius = 1.0f;
        float m_SpotlightCutoff = 1.0f;

        Renderer3D::LightHintData m_LightHintData;

        struct LightSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_PRIMITIVE(Type, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(Color, Serialization::DataType::Vec3);
            PINE_SERIALIZE_PRIMITIVE(Attenuation, Serialization::DataType::Vec3);
            PINE_SERIALIZE_PRIMITIVE(SpotlightRadius, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(SpotlightCutoff, Serialization::DataType::Float32);
        };
    public:
        Light();

        void SetLightType(LightType type);
        LightType GetLightType() const;

        void SetLightColor(Vector3f color);
        const Vector3f& GetLightColor() const;

        void SetLightAttenuation(Vector3f attenuation);
        const Vector3f& GetLightAttenuation() const;

        void SetSpotlightRadius(float radius);
        float GetSpotlightRadius() const;

        void SetSpotlightCutoff(float cutoff);
        float GetSpotlightCutoff() const;

        Renderer3D::LightHintData& GetLightHintData();

        void LoadData(const ByteSpan& span) override;
        ByteSpan SaveData() override;
    };

}

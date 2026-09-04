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

        // Linear radiance multiplier on the (linear) light color. Values > 1 push the lit result
        // above 1.0 in the HDR buffer, so the light can blow out / bloom instead of clamping to white.
        float m_Intensity = 1.0f;

        Vector3f m_LightAttenuation = Vector3f(1.f, 0.045f, 0.0075f);

        // Spotlight cone half-angles, in degrees. Inner is where the falloff starts, outer where
        // it reaches zero, so inner <= outer always. Stored as angles rather than the cosines the
        // shader wants: the conversion is one cos() at upload, and a cosine is a poor thing to put
        // in front of someone authoring a light.
        float m_SpotlightOuterAngle = 45.0f;
        float m_SpotlightInnerAngle = 30.0f;

        Renderer3D::LightHintData m_LightHintData;

        struct LightSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_PRIMITIVE(Type, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(Color, Serialization::DataType::Vec3);
            PINE_SERIALIZE_PRIMITIVE(Intensity, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(Attenuation, Serialization::DataType::Vec3);
            PINE_SERIALIZE_PRIMITIVE(SpotlightOuterAngle, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(SpotlightInnerAngle, Serialization::DataType::Float32);
        };
    public:
        Light();

        void SetLightType(LightType type);
        LightType GetLightType() const;

        void SetLightColor(Vector3f color);
        const Vector3f& GetLightColor() const;

        void SetLightIntensity(float intensity);
        float GetLightIntensity() const;

        void SetLightAttenuation(Vector3f attenuation);
        const Vector3f& GetLightAttenuation() const;

        // Both in degrees; the setters keep inner <= outer so the cone can never invert.
        void SetSpotlightOuterAngle(float degrees);
        float GetSpotlightOuterAngle() const;

        void SetSpotlightInnerAngle(float degrees);
        float GetSpotlightInnerAngle() const;

        Renderer3D::LightHintData& GetLightHintData();

        void LoadData(const ByteSpan& span) override;
        ByteSpan SaveData() override;
    };

}

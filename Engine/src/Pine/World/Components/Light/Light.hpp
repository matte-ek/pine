#pragma once
#include <optional>

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

            // Which ShadowView this light's shadow starts at, and how many it spans (1 for a spot,
            // 6 for a point light's cube faces). -1 means it is not casting this frame - either it
            // opted out, or it lost the contest for a shadow tile.
            //
            // Lives here because allocation happens once per frame at scene level (Pipeline3D::Prepare)
            // while the upload happens per rendering context (Renderer3D::AddLight), and the index has
            // to survive the gap between them.
            int ShadowViewIndex = -1;
            int ShadowViewCount = 0;

            // Where the light was when the objects' light slots were last checked against it, so
            // moving it can be told apart from turning it. Empty until the first check.
            std::optional<Vector3f> SlotPosition;
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

        // How far the light reaches, in world units. Not just a performance knob: the falloff
        // curve is windowed so it actually reaches zero here, which is what makes a shadow far
        // plane at this distance valid. Without that, light leaks past where its shadow map ends
        // and lights geometry the shadow pass never saw.
        float m_Range = 10.f;

        // Whether this light is a candidate for casting shadows. Not a promise that it will - the
        // shadow budget picks winners among the candidates - so this is the author's "this light is
        // cheap fill, never spend a tile on it" switch.
        bool m_CastShadows = true;

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
            PINE_SERIALIZE_PRIMITIVE(Range, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(CastShadows, Serialization::DataType::Boolean);
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

        // Raises the entity dirty flag: range feeds both light slot selection and, later, shadow
        // view invalidation.
        void SetRange(float range);
        float GetRange() const;

        void SetCastShadows(bool value);
        bool GetCastShadows() const;

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

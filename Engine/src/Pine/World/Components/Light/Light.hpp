#pragma once
#include "Pine/World/Components/IComponent/IComponent.hpp"
#include "Pine/Core/Math/Math.hpp"

namespace Pine
{

    enum class LightType
    {
        Directional,
        SpotLight,
        PointLight
    };

    class Light final  : public IComponent
    {
    private:
        LightType m_LightType = LightType::Directional;

        Vector3f m_LightColor = Vector3f(1.f, 1.f, 1.f);

        Vector3f m_LightAttenuation = Vector3f(1.f, 0.045f, 0.0075f);

        float m_SpotlightRadius = 1.0f;
        float m_SpotlightCutoff = 1.0f;
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

        void LoadData(const nlohmann::json& j) override;
        void SaveData(nlohmann::json& j) override;
    };

}

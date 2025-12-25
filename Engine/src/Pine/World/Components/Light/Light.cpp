#include "Light.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"

Pine::Light::Light()
        : IComponent(ComponentType::Light)
{
}

void Pine::Light::SetLightType(LightType type)
{
    m_LightType = type;
}

Pine::LightType Pine::Light::GetLightType() const
{
    return m_LightType;
}

void Pine::Light::SetLightColor(Vector3f color)
{
    m_LightColor = color;
}

const Pine::Vector3f &Pine::Light::GetLightColor() const
{
    return m_LightColor;
}

void Pine::Light::SetLightAttenuation(Vector3f attenuation)
{
    m_LightAttenuation = attenuation;
}

const Pine::Vector3f & Pine::Light::GetLightAttenuation() const
{
    return m_LightAttenuation;
}

void Pine::Light::SetSpotlightRadius(float radius)
{
    m_SpotlightRadius = radius;
}

float Pine::Light::GetSpotlightRadius() const
{
    return m_SpotlightRadius;
}

void Pine::Light::SetSpotlightCutoff(float cutoff)
{
    m_SpotlightCutoff = cutoff;
}

float Pine::Light::GetSpotlightCutoff() const
{
    return m_SpotlightCutoff;
}

Pine::Renderer3D::LightHintData& Pine::Light::GetLightHintData()
{
    return m_LightHintData;
}

void Pine::Light::LoadData(const nlohmann::json &j)
{
    Serialization::LoadValue(j, "lightType", m_LightType);
    Serialization::LoadVector3(j, "lightColor", m_LightColor);
    Serialization::LoadVector3(j, "lightAttenuation", m_LightAttenuation);
    Serialization::LoadValue(j, "spotlightRadius", m_SpotlightRadius);
    Serialization::LoadValue(j, "spotlightCutoff", m_SpotlightCutoff);
}

void Pine::Light::SaveData(nlohmann::json &j)
{
    j["lightType"] = static_cast<int>(m_LightType);
    j["lightColor"] = Serialization::StoreVector3(m_LightColor);
    j["lightAttenuation"] = Serialization::StoreVector3(m_LightAttenuation);
    j["spotlightRadius"] = m_SpotlightRadius;
    j["spotlightCutoff"] = m_SpotlightCutoff;
}

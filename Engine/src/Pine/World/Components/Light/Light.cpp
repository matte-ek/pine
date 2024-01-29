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

void Pine::Light::LoadData(const nlohmann::json &j)
{
    Serialization::LoadValue(j, "lightType", m_LightType);
    Serialization::LoadVector3(j, "lightColor", m_LightColor);
}

void Pine::Light::SaveData(nlohmann::json &j)
{
    j["lightType"] = static_cast<int>(m_LightType);
    j["lightColor"] = Serialization::StoreVector3(m_LightColor);
}

#include "Light.hpp"

Pine::Light::Light()
        : IComponent(ComponentType::Light)
{
}

void Pine::Light::SetLightType(Pine::LightType type)
{
    m_LightType = type;
}

Pine::LightType Pine::Light::GetLightType() const
{
    return m_LightType;
}

void Pine::Light::SetLightColor(Pine::Vector3f color)
{
    m_LightColor = color;
}

const Pine::Vector3f &Pine::Light::GetLightColor() const
{
    return m_LightColor;
}

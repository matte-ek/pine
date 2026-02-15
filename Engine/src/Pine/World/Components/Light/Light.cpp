#include "Light.hpp"
#include "../../../Core/Serialization/Json/SerializationJson.hpp"

Pine::Light::Light()
        : Component(ComponentType::Light)
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

void Pine::Light::LoadData(const ByteSpan& span)
{
    LightSerializer serializer;

    serializer.Read(span);

    serializer.Type.Read(m_LightType);
    serializer.Color.Read(m_LightColor);
    serializer.Attenuation.Read(m_LightAttenuation);
    serializer.SpotlightRadius.Read(m_SpotlightRadius);
    serializer.SpotlightCutoff.Read(m_SpotlightCutoff);
}

Pine::ByteSpan Pine::Light::SaveData()
{
    LightSerializer serializer;

    serializer.Type.Write(m_LightType);
    serializer.Color.Write(m_LightColor);
    serializer.Attenuation.Write(m_LightAttenuation);
    serializer.SpotlightRadius.Write(m_SpotlightRadius);
    serializer.SpotlightCutoff.Write(m_SpotlightCutoff);

    return serializer.Write();
}
#include "Light.hpp"

#include "Pine/World/Entity/Entity.hpp"

#include <algorithm>
#include "../../../Core/Serialization/Json/SerializationJson.hpp"

Pine::Light::Light()
        : Component(ComponentType::Light)
{
}

void Pine::Light::SetLightType(const LightType type)
{
    if (m_LightType == type)
    {
        return;
    }

    m_LightType = type;

    // Which object light slot this light competes for depends on its type, so the per-object slots
    // cached in ModelRendererHintData are stale until they're recomputed. Position changes are
    // already covered by the transform's own dirty flag; this is the other input to that cache.
    // SceneProcessor::Prepare clears the entity dirty flag once it has been acted on.
    if (auto* parent = GetParent())
    {
        parent->SetDirty(true);
    }
}

Pine::LightType Pine::Light::GetLightType() const
{
    return m_LightType;
}

void Pine::Light::SetLightColor(const Vector3f color)
{
    m_LightColor = color;
}

const Pine::Vector3f &Pine::Light::GetLightColor() const
{
    return m_LightColor;
}

void Pine::Light::SetLightIntensity(const float intensity)
{
    m_Intensity = intensity;
}

float Pine::Light::GetLightIntensity() const
{
    return m_Intensity;
}

void Pine::Light::SetRange(const float range)
{
    // A zero or negative range makes the falloff window degenerate and the shadow far plane
    // invalid, so it is clamped rather than trusted.
    const float newRange = std::max(range, 0.01f);

    if (m_Range == newRange)
    {
        return;
    }

    m_Range = newRange;

    // Same reasoning as SetLightType: range decides how far this light reaches, which feeds the
    // per-object slot cache and (later) shadow view invalidation. SceneProcessor::Prepare clears
    // the entity dirty flag once it has been acted on.
    if (auto* parent = GetParent())
    {
        parent->SetDirty(true);
    }
}

float Pine::Light::GetRange() const
{
    return m_Range;
}

void Pine::Light::SetCastShadows(const bool value)
{
    m_CastShadows = value;
}

bool Pine::Light::GetCastShadows() const
{
    return m_CastShadows;
}

// The cone is only well defined for 0 <= inner <= outer < 90. Clamping in the setters keeps the
// editor from producing a degenerate cone; Renderer3D::AddLight guards the upload as well, since
// LoadData writes the fields directly.
void Pine::Light::SetSpotlightOuterAngle(const float degrees)
{
    m_SpotlightOuterAngle = std::clamp(degrees, 1.f, 89.f);
    m_SpotlightInnerAngle = std::min(m_SpotlightInnerAngle, m_SpotlightOuterAngle);
}

float Pine::Light::GetSpotlightOuterAngle() const
{
    return m_SpotlightOuterAngle;
}

void Pine::Light::SetSpotlightInnerAngle(const float degrees)
{
    m_SpotlightInnerAngle = std::clamp(degrees, 0.f, m_SpotlightOuterAngle);
}

float Pine::Light::GetSpotlightInnerAngle() const
{
    return m_SpotlightInnerAngle;
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
    serializer.Intensity.Read(m_Intensity);
    serializer.Range.Read(m_Range);
    serializer.CastShadows.Read(m_CastShadows);
    serializer.SpotlightOuterAngle.Read(m_SpotlightOuterAngle);
    serializer.SpotlightInnerAngle.Read(m_SpotlightInnerAngle);

    // The type and range may have changed, and both feed the per-object light slot cache (see
    // SetLightType). A light still being loaded into a new entity has no parent to mark yet.
    if (auto* parent = GetParent())
    {
        parent->SetDirty(true);
    }
}

Pine::ByteSpan Pine::Light::SaveData()
{
    LightSerializer serializer;

    serializer.Type.Write(m_LightType);
    serializer.Color.Write(m_LightColor);
    serializer.Intensity.Write(m_Intensity);
    serializer.Range.Write(m_Range);
    serializer.CastShadows.Write(m_CastShadows);
    serializer.SpotlightOuterAngle.Write(m_SpotlightOuterAngle);
    serializer.SpotlightInnerAngle.Write(m_SpotlightInnerAngle);

    return serializer.Write();
}
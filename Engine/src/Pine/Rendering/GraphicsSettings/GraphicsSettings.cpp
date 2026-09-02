#include "GraphicsSettings.hpp"

#include <algorithm>

#include "Pine/Core/Log/Log.hpp"
#include "Pine/Core/Serialization/Json/SerializationJson.hpp"
#include "Pine/Rendering/Pipeline/Pipeline3D/Pipeline3D.hpp"

using namespace Pine;

namespace
{
    Rendering::GraphicsSettings::Settings m_Settings;

    constexpr const char* SETTINGS_FILE = "graphics.json";
}

void Rendering::GraphicsSettings::ApplyPreset(const QualityPreset preset)
{
    Settings& s = m_Settings;

    switch (preset)
    {
    case QualityPreset::Low:
        s.Shadows = false;
        s.AmbientOcclusion = false;
        s.AmbientOcclusionSamples = 8;
        s.AmbientOcclusionBlurPasses = 2;
        s.ShadowMapResolution = 1024;
        s.AmbientOcclusionResDivisor = 4;
        break;
    case QualityPreset::Medium:
        s.Shadows = true;
        s.AmbientOcclusion = true;
        s.AmbientOcclusionSamples = 16;
        s.AmbientOcclusionBlurPasses = 3;
        s.ShadowMapResolution = 2048;
        s.AmbientOcclusionResDivisor = 2;
        break;
    case QualityPreset::High:
        s.Shadows = true;
        s.AmbientOcclusion = true;
        s.AmbientOcclusionSamples = 24;
        s.AmbientOcclusionBlurPasses = 4;
        s.ShadowMapResolution = 4096;
        s.AmbientOcclusionResDivisor = 2;
        break;
    case QualityPreset::Ultra:
        s.Shadows = true;
        s.AmbientOcclusion = true;
        s.AmbientOcclusionSamples = 48;
        s.AmbientOcclusionBlurPasses = 4;
        s.ShadowMapResolution = 4096;
        s.AmbientOcclusionResDivisor = 1;
        break;
    case QualityPreset::Custom:
        // Leave the fields as-is.
        break;
    }

    s.Preset = preset;
}

void Rendering::GraphicsSettings::Setup()
{
    // Start from the High preset so a missing file gives sane defaults.
    ApplyPreset(QualityPreset::High);

    const auto json = SerializationJson::LoadFromFile(SETTINGS_FILE);

    if (!json.has_value())
    {
        PInfo("No graphics.json found, using default (High) graphics settings.");
        return;
    }

    const auto& j = json.value();

    int preset = static_cast<int>(m_Settings.Preset);
    SerializationJson::LoadValue(j, "preset", preset);
    m_Settings.Preset = static_cast<QualityPreset>(preset);

    SerializationJson::LoadValue(j, "shadows", m_Settings.Shadows);
    SerializationJson::LoadValue(j, "ambientOcclusion", m_Settings.AmbientOcclusion);
    SerializationJson::LoadValue(j, "ambientOcclusionSamples", m_Settings.AmbientOcclusionSamples);
    SerializationJson::LoadValue(j, "ambientOcclusionBlurPasses", m_Settings.AmbientOcclusionBlurPasses);
    SerializationJson::LoadValue(j, "shadowMapResolution", m_Settings.ShadowMapResolution);
    SerializationJson::LoadValue(j, "ambientOcclusionResDivisor", m_Settings.AmbientOcclusionResDivisor);
}

const Rendering::GraphicsSettings::Settings& Rendering::GraphicsSettings::Get()
{
    return m_Settings;
}

void Rendering::GraphicsSettings::Set(const Settings& settings)
{
    m_Settings = settings;
}

void Rendering::GraphicsSettings::Save()
{
    nlohmann::json j;

    j["preset"] = static_cast<int>(m_Settings.Preset);
    j["shadows"] = m_Settings.Shadows;
    j["ambientOcclusion"] = m_Settings.AmbientOcclusion;
    j["ambientOcclusionSamples"] = m_Settings.AmbientOcclusionSamples;
    j["ambientOcclusionBlurPasses"] = m_Settings.AmbientOcclusionBlurPasses;
    j["shadowMapResolution"] = m_Settings.ShadowMapResolution;
    j["ambientOcclusionResDivisor"] = m_Settings.AmbientOcclusionResDivisor;

    SerializationJson::SaveToFile(SETTINGS_FILE, j);
}

void Rendering::GraphicsSettings::ApplyRuntime()
{
    auto& config = Pipeline3D::GetPipelineConfiguration();

    config.RenderShadows = m_Settings.Shadows;
    config.RenderAmbientOcclusion = m_Settings.AmbientOcclusion;
}

int Rendering::GraphicsSettings::GetShadowMapResolution()
{
    return std::max(16, m_Settings.ShadowMapResolution);
}

int Rendering::GraphicsSettings::GetAmbientOcclusionResDivisor()
{
    return std::max(1, m_Settings.AmbientOcclusionResDivisor);
}

int Rendering::GraphicsSettings::GetAmbientOcclusionBlurPasses()
{
    return std::max(0, m_Settings.AmbientOcclusionBlurPasses);
}

int Rendering::GraphicsSettings::GetAmbientOcclusionSamples()
{
    return std::clamp(m_Settings.AmbientOcclusionSamples, 1, 64);
}

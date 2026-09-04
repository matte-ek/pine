#include "GraphicsSettingsPanel.hpp"

#include <imgui.h>

#include "IconsMaterialDesign.h"
#include "Gui/Shared/Widgets/Widgets.hpp"
#include "Pine/Rendering/GraphicsSettings/GraphicsSettings.hpp"

using Pine::Rendering::GraphicsSettings::QualityPreset;

namespace
{
    bool m_Active = false;

    // A working copy of the settings; committed to the engine on "Save & Apply".
    Pine::Rendering::GraphicsSettings::Settings m_Settings;

    // Discrete options for the allocation-class dropdowns.
    constexpr int m_ShadowResValues[] = { 512, 1024, 2048, 4096, 8192 };

    void SyncFromEngine()
    {
        m_Settings = Pine::Rendering::GraphicsSettings::Get();
    }
}

void Panels::GraphicsSettings::SetActive(bool value)
{
    m_Active = value;
}

bool Panels::GraphicsSettings::GetActive()
{
    return m_Active;
}

void Panels::GraphicsSettings::Setup()
{
    SyncFromEngine();
}

void Panels::GraphicsSettings::Render()
{
    if (!m_Active)
        return;

    if (ImGui::Begin(ICON_MD_DISPLAY_SETTINGS "  Graphics Settings", &m_Active))
    {
        // --- Quality preset ---
        int preset = static_cast<int>(m_Settings.Preset);
        if (Widgets::DropDown("Quality Preset", &preset, "Low\0" "Medium\0" "High\0" "Ultra\0" "Custom\0"))
        {
            const auto selected = static_cast<QualityPreset>(preset);
            if (selected != QualityPreset::Custom)
            {
                // Fill the working copy with the preset's values.
                Pine::Rendering::GraphicsSettings::ApplyPreset(selected);
                SyncFromEngine();
            }
            else
            {
                m_Settings.Preset = QualityPreset::Custom;
            }
        }

        // Tracks whether the user tweaked an individual field (=> Custom).
        bool changed = false;

        ImGui::SeparatorText("Live");

        changed |= Widgets::Checkbox("Shadows", &m_Settings.Shadows);
        changed |= Widgets::Checkbox("Ambient Occlusion", &m_Settings.AmbientOcclusion);
        changed |= Widgets::SliderInt("AO Samples", &m_Settings.AmbientOcclusionSamples, 1, 64);
        changed |= Widgets::SliderInt("AO Blur Passes", &m_Settings.AmbientOcclusionBlurPasses, 0, 6);

        ImGui::SeparatorText("Applied on restart");

        // One control, because there is one shadow texture. The cascades pin the two half-size
        // tiles out of it and the local lights compete for the rest, so this sets every shadow's
        // resolution at once - a directional map size that could be set independently of the atlas
        // no longer exists.
        int shadowIndex = 3; // default 4096
        for (int i = 0; i < 5; i++)
        {
            if (m_ShadowResValues[i] == m_Settings.ShadowAtlasResolution)
                shadowIndex = i;
        }
        if (Widgets::DropDown("Shadow Atlas Resolution", &shadowIndex, "512\0" "1024\0" "2048\0" "4096\0" "8192\0"))
        {
            m_Settings.ShadowAtlasResolution = m_ShadowResValues[shadowIndex];
            changed = true;
        }

        changed |= Widgets::SliderInt("Shadow Tile Budget", &m_Settings.LocalShadowTileBudget, 0, 16);

        changed |= Widgets::SliderInt("AO Resolution Divisor", &m_Settings.AmbientOcclusionResDivisor, 1, 4);

        // Any manual field edit means we no longer match a named preset.
        if (changed)
        {
            m_Settings.Preset = QualityPreset::Custom;
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Shadow atlas size and AO resolution divisor take effect after a restart.");
        ImGui::Spacing();

        if (ImGui::Button("Save & Apply", ImVec2(150, 40)))
        {
            Pine::Rendering::GraphicsSettings::Set(m_Settings);
            Pine::Rendering::GraphicsSettings::Save();
            // Live toggles (shadows, AO on/off) take effect immediately.
            Pine::Rendering::GraphicsSettings::ApplyRuntime();
        }

        ImGui::SameLine();

        if (ImGui::Button("Revert", ImVec2(150, 40)))
        {
            SyncFromEngine();
        }
    }
    ImGui::End();
}

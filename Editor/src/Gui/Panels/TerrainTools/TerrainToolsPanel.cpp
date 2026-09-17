#include "TerrainToolsPanel.hpp"

#include <imgui.h>

#include "IconsMaterialDesign.h"
#include "Gui/Shared/Widgets/Widgets.hpp"
#include "Other/PlayHandler/PlayHandler.hpp"
#include "Pine/Assets/Terrain/Terrain.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Components/TerrainRenderer/TerrainRendererComponent.hpp"

namespace
{
    using Editor::TerrainSculpting::BrushMode;

    bool m_Active = false;
    bool m_Editing = false;

    Editor::TerrainSculpting::Brush m_Brush;

    // Strength means a different thing in each of the brush's two halves - world units per second
    // while moving the ground, share of a layer per second while painting - and the two are an
    // order of magnitude apart. Kept separately so that switching to Paint does not carry a raise
    // brush's 32 units over as a stamp that covers everything under the cursor at once.
    float m_SculptStrength = 8.f;
    float m_PaintStrength = 2.f;

    // The reference height Flatten levels to, and whether the author has pinned one. Unpinned, a
    // stroke takes the height of the ground it started on - which is what clicking somewhere and
    // dragging means - so this is only read when the box below is ticked.
    bool m_UseFixedFlattenHeight = false;
    float m_FixedFlattenHeight = 0.f;

    // How many terrains there are to sculpt. Not a target to edit - the cursor picks that - but
    // without it a scene with no terrain at all just silently does nothing on click.
    int CountTerrains()
    {
        int count = 0;

        for (const auto& component : Pine::Components::Get<Pine::TerrainRendererComponent>())
        {
            if (component.GetTerrain() != nullptr)
            {
                count++;
            }
        }

        return count;
    }

    void RenderModeButtons()
    {
        const auto modeButton = [](const char* icon, const char* tooltip, const BrushMode mode)
        {
            const bool selected = m_Brush.Mode == mode;

            ImGui::PushStyleColor(ImGuiCol_Button, selected ? ImVec4(0.11f, 0.27f, 0.24f, 1.00f)
                                                            : ImVec4(0.04f, 0.15f, 0.11f, 1.00f));

            if (ImGui::Button(icon, ImVec2(60.f, 0.f)))
            {
                m_Brush.Mode = mode;
            }

            ImGui::PopStyleColor();

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", tooltip);
            }
        };

        modeButton(ICON_MD_ARROW_UPWARD, "Raise the ground under the brush", BrushMode::Raise);
        ImGui::SameLine();
        modeButton(ICON_MD_ARROW_DOWNWARD, "Lower the ground under the brush", BrushMode::Lower);
        ImGui::SameLine();
        modeButton(ICON_MD_WAVES, "Smooth towards the surrounding ground", BrushMode::Smooth);
        ImGui::SameLine();
        modeButton(ICON_MD_HORIZONTAL_RULE, "Flatten towards one height", BrushMode::Flatten);
        ImGui::SameLine();
        modeButton(ICON_MD_BRUSH, "Paint the chosen layer onto the ground", BrushMode::Paint);
    }

    // The terrain the layer names come from: the one in the scene, when there is exactly one.
    //
    // The panel has no target of its own - the cursor picks a terrain for each stroke - so naming a
    // layer is only honest while there is nothing in the scene to disagree with it. With several
    // terrains the buttons fall back to their channel numbers, which is what the brush writes
    // into regardless.
    Pine::Terrain* GetOnlyTerrain()
    {
        Pine::Terrain* only = nullptr;

        for (const auto& component : Pine::Components::Get<Pine::TerrainRendererComponent>())
        {
            const auto terrain = component.GetTerrain();

            if (terrain == nullptr)
            {
                continue;
            }

            if (only != nullptr && only != terrain)
            {
                return nullptr;
            }

            only = terrain;
        }

        return only;
    }

    void RenderLayerButtons()
    {
        const auto terrain = GetOnlyTerrain();

        for (int layer = 0; layer < Pine::Terrain::MaximumLayerCount; layer++)
        {
            const auto material = terrain != nullptr ? terrain->GetLayer(layer) : nullptr;
            const bool selected = m_Brush.Layer == layer;

            ImGui::PushStyleColor(ImGuiCol_Button, selected ? ImVec4(0.11f, 0.27f, 0.24f, 1.00f)
                                                            : ImVec4(0.04f, 0.15f, 0.11f, 1.00f));

            const auto label = material != nullptr
                ? fmt::format("{}  {}", layer, material->GetFileName())
                : fmt::format("{}", layer);

            if (ImGui::Button(label.c_str(), ImVec2(-1.f, 0.f)))
            {
                m_Brush.Layer = layer;
            }

            ImGui::PopStyleColor();
        }

        // Painting into an empty slot is allowed - the weights are the terrain's, and a material
        // can be assigned afterwards - but it changes nothing anybody can see, which is worth
        // saying out loud rather than leaving as a brush that appears to do nothing.
        if (terrain != nullptr && terrain->GetLayer(m_Brush.Layer) == nullptr)
        {
            ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.f),
                               "Layer %d has no material yet.", m_Brush.Layer);
        }
    }
}

void Panels::TerrainTools::SetActive(const bool value)
{
    m_Active = value;
}

bool Panels::TerrainTools::GetActive()
{
    return m_Active;
}

bool Panels::TerrainTools::IsEditing()
{
    // Sculpting writes to an asset and is undone through the editor's history, which refuses to run
    // while the game does - so a stroke applied now would be one the author could not take back.
    return m_Active && m_Editing &&
           PlayHandler::GetGameState() == PlayHandler::EditorGameState::Stopped;
}

const Editor::TerrainSculpting::Brush& Panels::TerrainTools::GetBrush()
{
    return m_Brush;
}

void Panels::TerrainTools::Render()
{
    if (!m_Active)
    {
        return;
    }

    if (ImGui::Begin(ICON_MD_LANDSCAPE "  Terrain Tools", &m_Active))
    {
        const int terrainCount = CountTerrains();

        ImGui::Checkbox("Edit terrain", &m_Editing);

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("While this is on, dragging in the Level viewport sculpts instead of\n"
                              "selecting and moving entities.");
        }

        if (m_Editing && terrainCount == 0)
        {
            ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.f),
                               "No terrain in the scene to sculpt.");
        }
        else if (m_Editing && PlayHandler::GetGameState() != PlayHandler::EditorGameState::Stopped)
        {
            ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.f),
                               "Stop play mode to sculpt.");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        RenderModeButtons();

        ImGui::Spacing();

        const bool painting = m_Brush.Mode == BrushMode::Paint;

        Widgets::SliderFloat("Radius", &m_Brush.Radius, 0.5f, 128.f, true);

        if (painting)
        {
            Widgets::SliderFloat("Strength", &m_PaintStrength, 0.05f, 8.f, true);

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("How much of the layer a second under the brush hands over.\n"
                                  "Coverage approaches full rather than reaching it, so holding\n"
                                  "still keeps blending towards the layer.");
            }
        }
        else
        {
            Widgets::SliderFloat("Strength", &m_SculptStrength, 0.1f, 64.f, true);

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("How far the ground moves per second under the brush, in world units.");
            }
        }

        Widgets::SliderFloat("Falloff", &m_Brush.Falloff, 0.f, 1.f);

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("How much of the radius is soft edge. 0 stamps with a hard rim,\n"
                              "1 is a dome peaking under the cursor.");
        }

        // Sliders do not hard-clamp - ctrl+click types any value - and a brush with no radius or a
        // negative strength is a stroke that quietly does nothing.
        m_Brush.Radius = std::max(m_Brush.Radius, 0.1f);
        m_SculptStrength = std::max(m_SculptStrength, 0.f);
        m_PaintStrength = std::max(m_PaintStrength, 0.f);
        m_Brush.Falloff = std::clamp(m_Brush.Falloff, 0.f, 1.f);

        m_Brush.Strength = painting ? m_PaintStrength : m_SculptStrength;

        if (m_Brush.Mode == BrushMode::Flatten)
        {
            ImGui::Spacing();

            ImGui::Checkbox("Flatten to a fixed height", &m_UseFixedFlattenHeight);

            if (m_UseFixedFlattenHeight)
            {
                Widgets::InputFloat("Height", &m_FixedFlattenHeight);
            }
            else
            {
                ImGui::TextDisabled("Levels to the ground each stroke starts on.");
            }
        }

        m_Brush.FlattenHeight = m_UseFixedFlattenHeight
            ? std::optional<float>(m_FixedFlattenHeight)
            : std::nullopt;

        if (painting)
        {
            ImGui::Spacing();
            ImGui::TextDisabled("Layer");

            RenderLayerButtons();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextDisabled(painting ? "Drag to paint. Each drag is one undo step."
                                     : "Drag to sculpt. Each drag is one undo step.");
        ImGui::TextDisabled("%d terrain(s) in the scene.", terrainCount);
    }

    ImGui::End();
}

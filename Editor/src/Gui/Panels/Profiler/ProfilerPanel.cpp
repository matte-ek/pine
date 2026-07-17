#include "ProfilerPanel.hpp"
#include "imgui.h"
#include "IconsMaterialDesign.h"
#include "Gui/Gui.hpp"
#include "Pine/Performance/Performance.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"

namespace
{
    bool m_Active = true;
    int m_SelectedScope = -1;

    bool m_HasCachedScopes = false;
    Pine::Performance::TrackedScope* m_RenderManagerScope = nullptr;

    void CacheTimedScopes()
    {
        // FindTrackedScopeByName is not ideal due to it being prone to break during refactoring.

        m_RenderManagerScope = Pine::Performance::FindTrackedScopeByName("void Pine::RenderManager::Run()");
    }

    void RenderTimedScopes()
    {
        auto& timedScopes = Pine::Performance::GetTrackedScopes();

        if (ImGui::BeginTable("##ProfilerTable", 2, ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Time (s)");
            ImGui::TableHeadersRow();

            for (const auto& scope : timedScopes)
            {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", scope->Name);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.5f sec", scope->Time);
            }

            ImGui::EndTable();
        }
    }
}

void Panels::Profiler::SetActive(bool value)
{
    m_Active = value;
}

bool Panels::Profiler::GetActive()
{
    return m_Active;
}

void Panels::Profiler::Render()
{
    if (!m_Active)
        return;

    if (ImGui::Begin(ICON_MD_SPEED "  Profiler", &m_Active))
    {
        if (!m_HasCachedScopes)
        {
            m_HasCachedScopes = true;
            CacheTimedScopes();
        }

        ImGui::Columns(2);

        RenderTimedScopes();

        ImGui::NextColumn();

        ImGui::PushFont(Editor::Gui::GetBoldFont());
        ImGui::Text("Renderer");
        ImGui::PopFont();

        // Frame time
        // Dynamic light count
        const auto deltaTime = Pine::RenderManager::GetGlobalDeltaTime();

        ImGui::Text("Frame time: %.5f (%d FPS)", deltaTime, static_cast<int>(1.0 / deltaTime));

        if (m_RenderManagerScope != nullptr)
        {
            ImGui::Text("Render time: %.5f", m_RenderManagerScope->Time);
        }

        ImGui::PushFont(Editor::Gui::GetBoldFont());
        ImGui::Text("Resources");
        ImGui::PopFont();

        // Entity Count
        // Component Count
        // Asset Count

        ImGui::PushFont(Editor::Gui::GetBoldFont());
        ImGui::Text("GPU Resources");
        ImGui::PopFont();

        // Texture Count
        // Mesh Count
        // Shader Count

        ImGui::Columns(1);
    }
    ImGui::End();
}

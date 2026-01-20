#include "ProfilerPanel.hpp"
#include "imgui.h"
#include "IconsMaterialDesign.h"
#include "Pine/Performance/Performance.hpp"

namespace
{
    bool m_Active = true;
    int m_SelectedScope = -1;

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

    if (ImGui::Begin(ICON_MD_SPEED " Profiler", &m_Active))
    {
        ImGui::Columns(2);

        RenderTimedScopes();

        ImGui::NextColumn();

        if (m_SelectedScope != -1)
        {

        }

        ImGui::Columns(1);
    }
    ImGui::End();
}

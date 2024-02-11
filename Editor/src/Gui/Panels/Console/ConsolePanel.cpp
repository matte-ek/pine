#include "ConsolePanel.hpp"
#include "imgui.h"
#include "IconsMaterialDesign.h"
#include "Pine/Core/Log/Log.hpp"

namespace
{
    bool m_Active = true;
    bool m_ViewVerbose = false;
}

void Panels::Console::SetActive(bool value)
{
    m_Active = value;
}

bool Panels::Console::GetActive()
{
    return m_Active;
}

void Panels::Console::Render()
{
    if (!m_Active)
        return;

    if (ImGui::Begin(ICON_MD_TERMINAL " Console", &m_Active))
    {
        const auto& messages = Pine::Log::GetLogMessages();

        ImGui::Checkbox("View Verbose", &m_ViewVerbose);

        ImGui::BeginChild("##ConsoleMessages", ImVec2(-1.f, -1.f), true, 0);

        for (const auto& message : messages)
        {
            if (!m_ViewVerbose && message.Type == Pine::LogSeverity::Verbose)
                continue;

            bool restoreColor = true;

            switch (message.Type)
            {
                case Pine::LogSeverity::Verbose:
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.f));
                    ImGui::Text("verbose:");
                    restoreColor = false;
                    break;
                case Pine::LogSeverity::Info:
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.f));
                    ImGui::Text("info:");
                    break;
                case Pine::LogSeverity::Warning:
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.5f, 0.0f, 1.f));
                    ImGui::Text("warning:");
                    break;
                case Pine::LogSeverity::Error:
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.f, 0.f, 1.f));
                    ImGui::Text("error:");
                    break;
                case Pine::LogSeverity::Fatal:
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.f, 0.f, 1.f));
                    ImGui::Text("fatal:");
                    restoreColor = false;
                    break;
            }

            ImGui::SameLine();

            if (restoreColor)
                ImGui::PopStyleColor();

            ImGui::Text("%s", message.Message.c_str());

            if (!restoreColor)
                ImGui::PopStyleColor();
        }

        ImGui::EndChild();
    }
    ImGui::End();
}


#include "ProfilerPanel.hpp"
#include "imgui.h"
#include "IconsMaterialDesign.h"

namespace
{
    bool m_Active = true;
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

    }
    ImGui::End();
}

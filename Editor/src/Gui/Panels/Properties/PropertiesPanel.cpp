#include "PropertiesPanel.hpp"
#include "imgui.h"
#include "IconsMaterialDesign.h"

namespace
{
    bool m_Active = true;
}

void Panels::Properties::SetActive(bool value)
{
    m_Active = value;
}

bool Panels::Properties::GetActive()
{
    return m_Active;
}

void Panels::Properties::Render()
{
    if (!ImGui::Begin(ICON_MD_BUILD " Properties", &m_Active))
    {
        ImGui::End();

        return;
    }



    ImGui::End();
}

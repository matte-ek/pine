#include "AssetBrowserPanel.hpp"
#include "imgui.h"
#include "IconsMaterialDesign.h"

namespace
{
    bool m_Active = true;
}

void Panels::AssetBrowser::SetActive(bool value)
{
    m_Active = value;
}

bool Panels::AssetBrowser::GetActive()
{
    return m_Active;
}

void Panels::AssetBrowser::Render()
{
    if (!ImGui::Begin(ICON_MD_FOLDER_OPEN " Asset Browser", &m_Active))
    {
        ImGui::End();

        return;
    }



    ImGui::End();
}

#include "PropertiesPanel.hpp"
#include "imgui.h"
#include "IconsMaterialDesign.h"

#include "AssetPropertiesRenderer/AssetPropertiesRenderer.hpp"
#include "EntityPropertiesRenderer/EntityPropertiesRenderer.hpp"
#include "Gui/Shared/Selection/Selection.hpp"

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

    if (!Selection::GetSelectedEntities().empty())
        EntityPropertiesPanel::Render(Selection::GetSelectedEntities().front());

    if (!Selection::GetSelectedAssets().empty())
        AssetPropertiesPanel::Render(Selection::GetSelectedAssets().front());

    ImGui::End();
}

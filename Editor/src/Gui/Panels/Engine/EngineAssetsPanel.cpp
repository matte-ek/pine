#include "EngineAssetsPanel.hpp"
#include "imgui.h"
#include "Pine/Assets/Assets.hpp"

namespace
{
    bool m_Active = false;
}

void Panels::EngineAssetsPanel::Render()
{
    if (!m_Active)
        return;

    if (ImGui::Begin("Engine Assets", &m_Active))
    {
        const auto& assets = Pine::Assets::GetAll();

        ImGui::Text("Engine Assets: %zu", assets.size());

        ImGui::Separator();

        for (const auto& [path, asset] : assets)
        {
            ImGui::Text("%s - %s", path.c_str(), Pine::AssetTypeToString(asset->GetType()));
        }
    }
    ImGui::End();
}

bool Panels::EngineAssetsPanel::GetActive()
{
    return m_Active;
}

void Panels::EngineAssetsPanel::SetActive(bool value)
{
    m_Active = value;
}

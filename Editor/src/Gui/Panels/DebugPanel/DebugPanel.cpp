#include "DebugPanel.hpp"

#include <cstdint>
#include <imgui.h>

#include "IconsMaterialDesign.h"
#include "Gui/Shared/Selection/Selection.hpp"
#include "Pine/Rendering/Features/AmbientOcclusion/AmbientOcclusion.hpp"
#include "Pine/Rendering/Pipeline/Pipeline2D/Pipeline2D.hpp"
#include "Pine/Rendering/Pipeline/Pipeline3D/Pipeline3D.hpp"

#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Components/Light/Light.hpp"

namespace
{
    bool m_Active = true;

    bool m_AmbientOcclusionTexture = false;
    bool m_DepthPositionTexture = false;
}

void Panels::Debug::SetActive(bool value)
{
    m_Active = value;
}

bool Panels::Debug::GetActive()
{
    return m_Active;
}

void Panels::Debug::Render()
{
    if (!m_Active)
        return;

    if (ImGui::Begin(ICON_MD_TROUBLESHOOT " Rendering", &m_Active))
    {
        ImGui::Text("Lightning");

        const auto& selectedEntities = Selection::GetSelectedEntities();
        if (!selectedEntities.empty())
        {
            auto selectedEntity = selectedEntities.front();

            if (auto modelRenderer = selectedEntity->GetComponent<Pine::ModelRenderer>())
            {
                auto& lightData = modelRenderer->GetRenderingHintData();

                ImGui::Text("Has Computed Light Data: %d", lightData.HasComputedData);

                // Point Lights
                for (int i = 0; i < 3;i++)
                {
                    if (auto light = lightData.LightSlotIndex[i].Get())
                    {
                        ImGui::Text("Point Light #%d: %s", i, light->GetParent()->GetName().c_str());
                    }
                    else
                    {
                        ImGui::Text("Point Light #%d: N/A", i);
                    }
                }

                // Spotlights
                if (auto light = lightData.LightSlotIndex[3].Get())
                {
                    ImGui::Text("Spot Light #1: %s", light->GetParent()->GetName().c_str());
                }
                else
                {
                    ImGui::Text("Spot Light #2: N/A");
                }

                if (ImGui::Button("Invalidate object lightning data"))
                {
                    lightData.HasComputedData = false;
                }
            }

            if (auto light = selectedEntity->GetComponent<Pine::Light>())
            {

            }
        }

        ImGui::Separator();

        ImGui::Text("Renderer3D");

        ImGui::Checkbox("View Ambient Occlusion", &m_AmbientOcclusionTexture);
        ImGui::Checkbox("View Position Texture", &m_DepthPositionTexture);
    }
    ImGui::End();

    if (m_AmbientOcclusionTexture)
    {
        if (ImGui::Begin(ICON_MD_TROUBLESHOOT " Ambient Occlusion", &m_AmbientOcclusionTexture))
        {
            const std::uint64_t id = *static_cast<std::uint32_t*>(Pine::Rendering::AmbientOcclusion::GetOutputTexture()->GetGraphicsIdentifier());
            ImGui::Image(reinterpret_cast<ImTextureID>(id), ImVec2(640, 360));
        }
        ImGui::End();
    }

    if (m_DepthPositionTexture)
    {
        if (ImGui::Begin(ICON_MD_TROUBLESHOOT " Position Texture", &m_DepthPositionTexture))
        {
            const std::uint64_t id = *static_cast<std::uint32_t*>(Pine::Pipeline3D::GetPositionTexture()->GetGraphicsIdentifier());
            ImGui::Image(reinterpret_cast<ImTextureID>(id), ImVec2(640, 360));
        }
        ImGui::End();
    }
}

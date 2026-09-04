#include "DebugPanel.hpp"

#include <cstdint>
#include <imgui.h>

#include "IconsMaterialDesign.h"

#include "Gui/Shared/Selection/Selection.hpp"
#include "Rendering/RenderHandler.hpp"

#include "Pine/Physics/Physics3D/Physics3D.hpp"
#include "Pine/Rendering/Features/AmbientOcclusion/AmbientOcclusion.hpp"
#include "Pine/Rendering/Pipeline/Pipeline3D/Pipeline3D.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"

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

    if (ImGui::Begin(ICON_MD_TROUBLESHOOT "  Debug", &m_Active))
    {
        if (ImGui::CollapsingHeader("Culling"))
        {
            // Both contexts are listed on purpose: visibility is per frustum, so a level viewport
            // and a game camera pointed different ways should disagree here. If the two rows move
            // in lockstep regardless of where each camera looks, culling has regressed to being
            // shared again.
            const auto renderContextStats = [](const char* name, const Pine::RenderingContext* context)
            {
                if (context == nullptr)
                {
                    return;
                }

                const auto& statistics = context->Statistics;
                const int total = statistics.VisibleObjectCount + statistics.CulledObjectCount;

                ImGui::Text("%s: %d visible, %d culled (of %d)",
                    name,
                    statistics.VisibleObjectCount,
                    statistics.CulledObjectCount,
                    total);
            };

            renderContextStats("Level viewport", Editor::RenderHandler::GetLevelRenderingContext());
            renderContextStats("Game viewport", Editor::RenderHandler::GetGameRenderingContext());
        }

        if (ImGui::CollapsingHeader("Lightning"))
        {

            const auto& selectedEntities = Selection::GetSelectedEntities();
            if (!selectedEntities.empty())
            {
                auto selectedEntity = selectedEntities.front();

                if (auto modelRenderer = selectedEntity->GetComponent<Pine::ModelRenderer>())
                {
                    auto& lightData = modelRenderer->GetRenderingHintData();

                    ImGui::Text("Has Computed Light Data: %d", lightData.HasComputedData);

                    namespace Slots = Pine::Renderer3D::Specifications::ObjectLightSlots;

                    // Point Lights
                    for (int i = 0; i < Slots::POINT_LIGHT_COUNT;i++)
                    {
                        if (auto light = lightData.LightSlotIndex[Slots::POINT_LIGHT_OFFSET + i].Get())
                        {
                            ImGui::Text("Point Light #%d: %s", i, light->GetParent()->GetName().c_str());
                        }
                        else
                        {
                            ImGui::Text("Point Light #%d: N/A", i);
                        }
                    }

                    // Spotlights
                    for (int i = 0; i < Slots::SPOT_LIGHT_COUNT;i++)
                    {
                        if (auto light = lightData.LightSlotIndex[Slots::SPOT_LIGHT_OFFSET + i].Get())
                        {
                            ImGui::Text("Spot Light #%d: %s", i, light->GetParent()->GetName().c_str());
                        }
                        else
                        {
                            ImGui::Text("Spot Light #%d: N/A", i);
                        }
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

        if (ImGui::CollapsingHeader("Physics"))
        {
            if (ImGui::Button("Connect to PhysX debugger"))
            {
                Pine::Physics3D::ConnectVisualDebugger();
            }
        }

    }
    ImGui::End();

    if (m_AmbientOcclusionTexture)
    {
        if (ImGui::Begin(ICON_MD_TROUBLESHOOT " Ambient Occlusion", &m_AmbientOcclusionTexture))
        {
            const std::uint64_t id = *static_cast<std::uint32_t*>(Pine::Rendering::AmbientOcclusion::GetOutputTexture()->GetGraphicsIdentifier());
            ImGui::Image(id, ImVec2(640, 360));
        }
        ImGui::End();
    }

    if (m_DepthPositionTexture)
    {
        if (ImGui::Begin(ICON_MD_TROUBLESHOOT " Position Texture", &m_DepthPositionTexture))
        {
            const std::uint64_t id = *static_cast<std::uint32_t*>(Pine::Pipeline3D::GetPositionTexture()->GetGraphicsIdentifier());
            ImGui::Image(id, ImVec2(640, 360));
        }
        ImGui::End();
    }
}

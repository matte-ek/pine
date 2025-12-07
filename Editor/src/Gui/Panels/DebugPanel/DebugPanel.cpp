#include "DebugPanel.hpp"

#include <cstdint>
#include <imgui.h>

#include "IconsMaterialDesign.h"
#include "Pine/Rendering/Features/AmbientOcclusion/AmbientOcclusion.hpp"
#include "Pine/Rendering/Pipeline/Pipeline2D/Pipeline2D.hpp"
#include "Pine/Rendering/Pipeline/Pipeline3D/Pipeline3D.hpp"

namespace
{
    bool m_Active = true;

    bool m_AmbientOcclusionTexture = true;
    bool m_DepthPositionTexture = true;
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

    if (ImGui::Begin(ICON_MD_TROUBLESHOOT " Debug", &m_Active))
    {
        ImGui::Checkbox("Ambient Occlusion", &m_AmbientOcclusionTexture);
        ImGui::Checkbox("Position Texture", &m_DepthPositionTexture);
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

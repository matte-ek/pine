#include "GameViewportPanel.hpp"
#include "Rendering/RenderHandler.hpp"
#include "imgui.h"
#include "IconsMaterialDesign.h"

namespace
{
    bool m_Active = true;
    bool m_Visible = false;

    Pine::Vector2i m_Size = Pine::Vector2i(0);
}

void Panels::GameViewport::SetActive(bool value)
{
    m_Active = value;
}

bool Panels::GameViewport::GetActive()
{
    return m_Active;
}

bool Panels::GameViewport::GetVisible()
{
    return m_Visible;
}

Pine::Vector2i Panels::GameViewport::GetSize()
{
    return m_Size;
}

void Panels::GameViewport::Render()
{
    if (!ImGui::Begin(ICON_MD_SPORTS_ESPORTS " Game", &m_Active))
    {
        m_Visible = false;

        ImGui::End();

        return;
    }

    m_Visible = true;

    const auto avSize = ImGui::GetContentRegionAvail();
    const auto id = *reinterpret_cast<std::uint32_t*>(RenderHandler::GetGameFrameBuffer()->GetColorBuffer()->GetGraphicsIdentifier());

    m_Size = Pine::Vector2i(avSize.x, avSize.y);

    ImGui::Image(reinterpret_cast<ImTextureID>(id), avSize);

    ImGui::End();
}

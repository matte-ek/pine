#include "LevelViewportPanel.hpp"
#include "Rendering/RenderHandler.hpp"
#include "imgui.h"
#include "IconsMaterialDesign.h"

namespace
{
    bool m_Active = true;
    bool m_Visible = false;

    Pine::Vector2i m_Size = Pine::Vector2i(0);
}

void Panels::LevelViewport::SetActive(bool value)
{
    m_Active = value;
}

bool Panels::LevelViewport::GetActive()
{
    return m_Active;
}

bool Panels::LevelViewport::GetVisible()
{
    return m_Visible;
}

Pine::Vector2i Panels::LevelViewport::GetSize()
{
    return m_Size;
}

void Panels::LevelViewport::Render()
{
    if (!ImGui::Begin(ICON_MD_PUBLIC " Level", &m_Active))
    {
        m_Visible = false;

        ImGui::End();

        return;
    }

    m_Visible = true;

    const auto avSize = ImGui::GetContentRegionAvail();
    const auto id = *reinterpret_cast<std::uint32_t*>(RenderHandler::GetLevelFrameBuffer()->GetColorBuffer()->GetGraphicsIdentifier());

    m_Size = Pine::Vector2i(avSize.x, avSize.y);

    ImGui::Image(reinterpret_cast<ImTextureID>(id), avSize);

    ImGui::End();
}

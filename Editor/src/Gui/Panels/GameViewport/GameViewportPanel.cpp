#include "GameViewportPanel.hpp"
#include "Rendering/RenderHandler.hpp"
#include "imgui.h"
#include "IconsMaterialDesign.h"
#include "Pine/Input/Input.hpp"
#include "Other/PlayHandler/PlayHandler.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/World/World.hpp"

namespace
{
    bool m_Active = true;
    bool m_Visible = false;
    bool m_CaptureMouse = false;

    Pine::Vector2i m_Size = Pine::Vector2i(0);

    void ReleaseCapture()
    {
        if (m_CaptureMouse)
        {
            m_CaptureMouse = false;
            Pine::Input::SetCursorMode(Pine::CursorMode::Normal);
        }
    }
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
        ReleaseCapture();
        ImGui::End();
        return;
    }

    m_Visible = true;

    if (PlayHandler::GetGameState() == PlayHandler::EditorGameState::Stopped)
    {
        if (ImGui::Button(ICON_MD_PLAY_ARROW, ImVec2(45.f, 24.f)))
            PlayHandler::Play();
    }
    else
    {
        if (ImGui::Button(ICON_MD_STOP, ImVec2(45.f, 24.f)))
            PlayHandler::Stop();
    }

    const auto avSize = ImGui::GetContentRegionAvail();
    const auto renderScale = RenderHandler::GetGameRenderingContext()->Size /
                             static_cast<Pine::Vector2f>(RenderHandler::GetGameFrameBuffer()->GetSize());
    const std::uint64_t id =
        *static_cast<std::uint32_t*>(RenderHandler::GetGameFrameBuffer()->GetColorBuffer()->GetGraphicsIdentifier());

    m_Size = Pine::Vector2i(avSize.x, avSize.y);

    ImGui::Image(reinterpret_cast<ImTextureID>(id), avSize,
                 ImVec2(0.f, renderScale.y), ImVec2(renderScale.x, 0.f));

    const bool viewportClickedLeft = ImGui::IsItemClicked(ImGuiMouseButton_Left);

    const bool isPlaying = PlayHandler::GetGameState() != PlayHandler::EditorGameState::Stopped;

    if (isPlaying)
    {
        // Start capture on first click inside the viewport, or keep it if already captured
        if (!m_CaptureMouse && viewportClickedLeft)
        {
            m_CaptureMouse = true;
        }

        if (m_CaptureMouse)
        {
            Pine::Input::SetCursorMode(Pine::CursorMode::Disabled);

            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                ReleaseCapture();
        }

        if (!m_Visible || !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
            ReleaseCapture();
    }
    else
    {
        ReleaseCapture();
    }

    if (ImGui::BeginDragDropTarget())
    {
        if (ImGui::AcceptDragDropPayload("Asset", ImGuiDragDropFlags_SourceAllowNullID))
        {
            const auto asset = *static_cast<Pine::IAsset**>(ImGui::GetDragDropPayload()->Data);

            if (asset->GetType() == Pine::AssetType::Level)
            {
                if (auto level = dynamic_cast<Pine::Level*>(asset))
                {
                    Pine::World::SetActiveLevel(level);
                }
            }
        }

        ImGui::EndDragDropTarget();
    }

    Pine::Input::GetDefaultContext()->InputEnabled = m_CaptureMouse;

    ImGui::End();
}

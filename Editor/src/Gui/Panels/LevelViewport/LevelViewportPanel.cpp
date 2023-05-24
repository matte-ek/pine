#include "LevelViewportPanel.hpp"

#include "Rendering/RenderHandler.hpp"
#include "IconsMaterialDesign.h"
#include "EditorEntity/EditorEntity.hpp"
#include "Gui/Shared/Gizmo/Gizmo2D/Gizmo2D.hpp"
#include "Gui/Shared/Selection/Selection.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"

#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>

namespace
{
    bool m_Active = true;
    bool m_Visible = false;

    Pine::Vector2i m_Size = Pine::Vector2i(0);

    void RenderGuizmo()
    {
        static auto camera = EditorEntity::Get()->GetComponent<Pine::Camera>();

        if (Selection::GetSelectedEntities().empty())
            return;

        auto selectedEntity = Selection::GetSelectedEntities()[0];

        Pine::Matrix4f matrix = selectedEntity->GetTransform()->GetTransformationMatrix();

        if (ImGuizmo::Manipulate(glm::value_ptr(camera->GetViewMatrix()), glm::value_ptr(camera->GetProjectionMatrix()), ImGuizmo::OPERATION::TRANSLATE, ImGuizmo::MODE::WORLD, glm::value_ptr(matrix)))
        {
        }
    }
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
    const auto position = ImGui::GetCursorScreenPos();

    const std::uint64_t id = *static_cast<std::uint32_t*>(RenderHandler::GetLevelFrameBuffer()->GetColorBuffer()->GetGraphicsIdentifier());

    m_Size = Pine::Vector2i(avSize.x, avSize.y);

    ImGui::Image(reinterpret_cast<ImTextureID>(id), avSize, ImVec2(0.f, 1.f), ImVec2(1.f, 0.f));

    RenderGuizmo();

    ImGui::End();
}

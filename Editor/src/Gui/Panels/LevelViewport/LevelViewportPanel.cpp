#include "LevelViewportPanel.hpp"

#include "Rendering/RenderHandler.hpp"
#include "IconsMaterialDesign.h"
#include "EditorEntity/EditorEntity.hpp"
#include "Gui/Shared/Gizmo/Gizmo2D/Gizmo2D.hpp"
#include "Gui/Shared/Selection/Selection.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/Input/Input.hpp"

#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>

namespace
{
    bool m_Active = true;
    bool m_Visible = false;
    bool m_CaptureMouse = false;

    Pine::Vector2i m_MouseCapturePosition = Pine::Vector2i(0);
    Pine::Vector2i m_Size = Pine::Vector2i(0);

    enum class GizmoMode : int
    {
        Translate,
        Rotate,
        Scale
    };

    enum class PerspectiveMode : int
    {
        Local,
        World
    };

    GizmoMode m_GizmoMode;
    PerspectiveMode m_PerspectiveMode;

    void RenderGizmo(ImVec2 viewportPosition)
    {
        static auto camera = EditorEntity::Get()->GetComponent<Pine::Camera>();

        if (Selection::GetSelectedEntities().empty())
            return;

        auto selectedEntity = Selection::GetSelectedEntities()[0];
        auto transform = selectedEntity->GetTransform();

        Pine::Matrix4f matrix = transform->GetTransformationMatrix();
        Pine::Matrix4f deltaMatrix;

        ImGuizmo::SetRect(viewportPosition.x, viewportPosition.y, m_Size.x, m_Size.y);
        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());

        // Because ImGuizmo has problems respecting the max view port width and height for whatever reason.
        ImGui::GetWindowDrawList()->PushClipRect(ImVec2(viewportPosition.x, viewportPosition.y),
                                                 ImVec2(viewportPosition.x + m_Size.x, viewportPosition.y + m_Size.y));

        ImGuizmo::OPERATION operation;
        ImGuizmo::MODE mode = m_PerspectiveMode == PerspectiveMode::World ? ImGuizmo::WORLD : ImGuizmo::LOCAL;

        switch (m_GizmoMode)
        {
            case GizmoMode::Translate:
                operation = ImGuizmo::OPERATION::TRANSLATE;
                break;
            case GizmoMode::Rotate:
                operation = ImGuizmo::OPERATION::ROTATE;
                break;
            case GizmoMode::Scale:
                operation = ImGuizmo::OPERATION::SCALE;
                break;
        }

        if (ImGuizmo::Manipulate(glm::value_ptr(camera->GetViewMatrix()),
                                 glm::value_ptr(camera->GetProjectionMatrix()),
                                 operation,
                                 mode,
                                 glm::value_ptr(matrix),
                                 glm::value_ptr(deltaMatrix)))
        {

            Pine::Vector3f position, scale, skew;
            Pine::Vector4f perspective;
            Pine::Quaternion rotation;

            glm::decompose(matrix, scale, rotation, position, skew, perspective);

            if (m_GizmoMode == GizmoMode::Translate)
                transform->LocalPosition = position;
            if (m_GizmoMode == GizmoMode::Rotate)
                transform->LocalRotation = rotation;
            if (m_GizmoMode == GizmoMode::Scale)
                transform->LocalScale = scale;
        }

        ImGui::GetWindowDrawList()->PopClipRect();
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

    const std::uint64_t id = *static_cast<std::uint32_t*>(RenderHandler::GetLevelFrameBuffer()->GetColorBuffer()->GetGraphicsIdentifier());

    if (ImGui::Button("Translate"))
        m_GizmoMode = GizmoMode::Translate;

    ImGui::SameLine();

    if (ImGui::Button("Rotate"))
        m_GizmoMode = GizmoMode::Rotate;

    ImGui::SameLine();

    if (ImGui::Button("Scale"))
        m_GizmoMode = GizmoMode::Scale;

    ImGui::SameLine();

    ImGui::SetNextItemWidth(80.f);
    ImGui::Combo("##Mode", reinterpret_cast<int*>(&m_PerspectiveMode), "Local\0World\0");

    const auto avSize = ImGui::GetContentRegionAvail();
    const auto position = ImGui::GetCursorScreenPos();

    m_Size = Pine::Vector2i(avSize.x, avSize.y);

    ImGui::Image(reinterpret_cast<ImTextureID>(id), avSize, ImVec2(0.f, 1.f), ImVec2(1.f, 0.f));

    if (!m_CaptureMouse)
    {
        if (ImGui::IsItemClicked(ImGuiMouseButton_::ImGuiMouseButton_Right))
        {
            m_CaptureMouse = true;
            m_MouseCapturePosition = Pine::Input::GetCursorPosition();

            Pine::Input::SetCursorVisible(false);
        }
    }
    else
    {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_::ImGuiMouseButton_Right))
        {
            m_CaptureMouse = false;
            Pine::Input::SetCursorVisible(true);
        }

        Pine::Input::SetCursorPosition(m_MouseCapturePosition);
    }

    EditorEntity::SetCaptureMouse(m_CaptureMouse);

    RenderGizmo(position);

    ImGui::End();
}

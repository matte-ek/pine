#include "LevelViewportPanel.hpp"

#include "Rendering/RenderHandler.hpp"
#include "IconsMaterialDesign.h"
#include "EditorEntity/EditorEntity.hpp"
#include "Gui/Shared/Selection/Selection.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/Input/Input.hpp"
#include "Pine/World/World.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"

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

    enum class SnapMode : int
    {
        Disabled,
        OnKey,
        Always
    };

    GizmoMode m_GizmoMode = GizmoMode::Translate;
    PerspectiveMode m_PerspectiveMode = PerspectiveMode::Local;
    SnapMode m_SnapMode = SnapMode::OnKey;

    float m_SnapRange = 1.f;

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

        const auto shouldSnap = m_SnapMode == SnapMode::Always || (m_SnapMode == SnapMode::OnKey && ImGui::GetIO().KeyCtrl);
        auto snapRange = Pine::Vector3f(m_SnapRange);

        if (ImGuizmo::Manipulate(glm::value_ptr(camera->GetViewMatrix()),
                                 glm::value_ptr(camera->GetProjectionMatrix()),
                                 operation,
                                 mode,
                                 glm::value_ptr(matrix),
                                 glm::value_ptr(deltaMatrix),
                                 shouldSnap ? glm::value_ptr(snapRange) : nullptr))
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

            transform->OnRender(0.f);
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

    static auto tabButton = [](const char* text, bool selected)
    {
        bool ret = false;

        if (!selected)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.04f, 0.15f, 0.11f, 1.00f));
        else
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.11f, 0.27f, 0.24f, 1.00f));

        if (ImGui::Button(text, ImVec2(45.f, 24.f)))
            ret = true;

        ImGui::PopStyleColor();

        return ret;
    };

    if (tabButton(ICON_MD_OPEN_WITH, m_GizmoMode == GizmoMode::Translate))
        m_GizmoMode = GizmoMode::Translate;

    ImGui::SameLine();

    if (tabButton(ICON_MD_360, m_GizmoMode == GizmoMode::Rotate))
        m_GizmoMode = GizmoMode::Rotate;

    ImGui::SameLine();

    if (tabButton(ICON_MD_ZOOM_OUT_MAP, m_GizmoMode == GizmoMode::Scale))
        m_GizmoMode = GizmoMode::Scale;

    ImGui::SameLine();

    if (ImGui::Button(ICON_MD_EXPAND_MORE))
    {
        ImGui::OpenPopup("GizmoContextMenu");
    }

    ImGui::SetNextWindowSize(ImVec2(150.f, 0.f));
    if (ImGui::BeginPopup("GizmoContextMenu"))
    {
        if (ImGui::MenuItem("World Space", nullptr, m_PerspectiveMode == PerspectiveMode::World))
            m_PerspectiveMode = PerspectiveMode::World;
        if (ImGui::MenuItem("Local Space", nullptr, m_PerspectiveMode == PerspectiveMode::Local))
            m_PerspectiveMode = PerspectiveMode::Local;

        ImGui::Separator();

        if (ImGui::BeginMenu("Grid Snap"))
        {
            if (ImGui::MenuItem("Off", nullptr, m_SnapMode == SnapMode::Disabled))
                m_SnapMode = SnapMode::Disabled;
            if (ImGui::MenuItem("On Key", nullptr, m_SnapMode == SnapMode::OnKey))
                m_SnapMode = SnapMode::OnKey;
            if (ImGui::MenuItem("Always", nullptr, m_SnapMode == SnapMode::Always))
                m_SnapMode = SnapMode::Always;

            ImGui::SetNextItemWidth(150.f);
            ImGui::SliderFloat("Size", &m_SnapRange, 0.1f, 10.f, "%.1f");
            ImGui::EndMenu();
        }

        ImGui::EndPopup();
    }

    ImGui::Spacing();

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

    // Allow drag dropping some assets to the viewport such as blueprints, levels and sky boxes.
    if (ImGui::BeginDragDropTarget())
    {
        if (ImGui::AcceptDragDropPayload("Asset", ImGuiDragDropFlags_SourceAllowNullID))
        {
            const auto asset = *static_cast<Pine::IAsset**>(ImGui::GetDragDropPayload()->Data);

            if (asset->GetType() == Pine::AssetType::Texture3D)
            {
                auto currentLevel = Pine::World::GetActiveLevel();
                auto skybox = dynamic_cast<Pine::Texture3D*>(asset);

                if (currentLevel && skybox)
                    currentLevel->GetLevelSettings().Skybox = skybox;
            }

            if (asset->GetType() == Pine::AssetType::Level)
            {
                auto level = dynamic_cast<Pine::Level*>(asset);

                if (level)
                    Pine::World::SetActiveLevel(level);
            }
        }

        ImGui::EndDragDropTarget();
    }

    EditorEntity::SetCaptureMouse(m_CaptureMouse);

    RenderGizmo(position);

    ImGui::End();
}

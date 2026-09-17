#include "LevelViewportPanel.hpp"

#include "Rendering/RenderHandler.hpp"
#include "IconsMaterialDesign.h"
#include "Other/EditorEntity/EditorEntity.hpp"
#include "Gui/Shared/Selection/Selection.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/Input/Input.hpp"
#include "Pine/World/World.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Other/EntitySelection/EntitySelection.hpp"
#include "Other/PlayHandler/PlayHandler.hpp"
#include "Gui/Shared/Gizmo/Gizmo2D/Gizmo2D.hpp"
#include "Gui/Shared/Gizmo/Gizmo3D/Gizmo3D.hpp"

#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include "Other/Actions/Actions.hpp"
#include "Other/TerrainSculpting/TerrainSculpting.hpp"
#include "Gui/Panels/TerrainTools/TerrainToolsPanel.hpp"
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/Assets/Terrain/Terrain.hpp"
#include "Pine/Rendering/Features/TerrainRenderer/TerrainRenderer.hpp"
#include "Pine/World/Components/Collider/Collider.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Components/RigidBody/RigidBody.hpp"

namespace
{
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

    bool m_Active = true;
    bool m_Visible = false;
    bool m_CaptureMouse = false;

    auto m_MouseCapturePosition = Pine::Vector2i(0);
    auto m_Size = Pine::Vector2i(0);

    bool m_ShowEntitySpeed = false;
    double m_ShowEntitySpeedTime = 0.0;

    auto m_GizmoMode = GizmoMode::Translate;
    auto m_PerspectiveMode = PerspectiveMode::Local;
    auto m_SnapMode = SnapMode::OnKey;

    float m_SnapRange = 1.f;

    // True from a click on the viewport image until the button comes back up. A stroke needs this
    // rather than "the button is down": a drag that began on a slider in the tools panel and
    // wandered over the ground must not start sculpting, and a drag that began on the ground and
    // crossed a patch of sky must not end there.
    bool m_SculptDragging = false;

    // The cursor's position inside the viewport image, in its own pixels.
    Pine::Vector2f GetCursorInViewport(const ImVec2 viewportPosition)
    {
        const auto cursor = ImGui::GetMousePos();

        return { cursor.x - viewportPosition.x, cursor.y - viewportPosition.y };
    }

    // Runs the sculpting brush for this frame: the overlay under the cursor, and a stroke while the
    // button is held. Called instead of the gizmo and entity picking, never alongside them.
    void UpdateTerrainSculpting(const ImVec2 viewportPosition, const bool hovered, const bool clicked)
    {
        namespace Sculpting = Editor::TerrainSculpting;

        if (clicked)
        {
            m_SculptDragging = true;
        }

        // Ended on the button coming up wherever the cursor is by then, so releasing outside the
        // viewport cannot leave a stroke open and merge the next one into it.
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            m_SculptDragging = false;

            Sculpting::EndStroke();
        }

        const auto clearOverlay = []
        {
            Pine::Rendering::TerrainRenderer::SetBrushOverlay(std::nullopt);
        };

        // m_CaptureMouse is the right-button camera fly, which hides the cursor - there is nothing
        // to put a ring under.
        if (!hovered || m_CaptureMouse)
        {
            clearOverlay();
            return;
        }

        const auto ray = Sculpting::BuildCursorRay(Editor::LevelEntity::Get()->GetComponent<Pine::Camera>(),
                                                   GetCursorInViewport(viewportPosition),
                                                   m_Size);

        if (!ray.has_value())
        {
            clearOverlay();
            return;
        }

        const auto pick = Sculpting::PickTerrain(ray->Origin, ray->Direction);

        if (!pick.has_value())
        {
            clearOverlay();
            return;
        }

        const auto& brush = Panels::TerrainTools::GetBrush();
        const Pine::Vector2f point = { pick->LocalPosition.x, pick->LocalPosition.z };

        Pine::Rendering::TerrainRenderer::BrushOverlay overlay;

        overlay.Terrain = pick->Terrain->GetUId();
        overlay.Centre = point;
        overlay.Radius = brush.Radius;

        // Proportional to the brush, with a floor so a small one still reads as a ring rather than
        // as a solid disc.
        overlay.RingWidth = std::max(brush.Radius * 0.08f, 0.2f);

        Pine::Rendering::TerrainRenderer::SetBrushOverlay(overlay);

        if (m_SculptDragging)
        {
            Sculpting::Apply(pick->Terrain, brush, point, ImGui::GetIO().DeltaTime);
        }
    }

    void RenderTranslationGizmo(ImVec2 viewportPosition)
    {
        static auto camera = Editor::LevelEntity::Get()->GetComponent<Pine::Camera>();

        if (Selection::GetSelectedEntities().empty())
            return;

        auto selectedEntity = Selection::GetSelectedEntities()[0];
        auto transform = selectedEntity->GetTransform();

        transform->SetDirty();

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
            default:
                return;
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
            Editor::Actions::CreateComponentCommand command(transform, Editor::Actions::CommandType::Update, true);

            Pine::Vector3f position, scale, skew;
            Pine::Vector4f perspective;
            Pine::Quaternion rotation;

            selectedEntity->SetDirty(true);

            if (selectedEntity->GetParent() != nullptr)
            {
                glm::decompose(deltaMatrix, scale, rotation, position, skew, perspective);

                if (m_GizmoMode == GizmoMode::Translate)
                    transform->SetLocalPosition(transform->GetLocalPosition() + position);
                if (m_GizmoMode == GizmoMode::Rotate)
                    transform->SetLocalRotation(transform->GetLocalRotation() * rotation);
            }
            else
            {
                glm::decompose(matrix, scale, rotation, position, skew, perspective);

                if (m_GizmoMode == GizmoMode::Translate)
                    transform->SetLocalPosition(position);
                if (m_GizmoMode == GizmoMode::Rotate)
                    transform->SetLocalRotation(rotation);
                if (m_GizmoMode == GizmoMode::Scale)
                    transform->SetLocalScale(scale);
            }

            if (Selection::GetSelectedEntities().size() > 1)
            {
                glm::decompose(deltaMatrix, scale, rotation, position, skew, perspective);

                for (auto entity : Selection::GetSelectedEntities())
                {
                    if (entity == selectedEntity)
                        continue;

                    auto entityTransform = entity->GetTransform();

                    if (m_GizmoMode == GizmoMode::Translate)
                        entityTransform->SetLocalPosition(entityTransform->GetLocalPosition() + position);
                    if (m_GizmoMode == GizmoMode::Rotate)
                        entityTransform->SetLocalRotation(entityTransform->GetLocalRotation() * rotation);
                    if (m_GizmoMode == GizmoMode::Scale)
                        entityTransform->SetLocalScale(entityTransform->GetLocalScale() + scale);

                    entityTransform->OnRender(0.f);
                    entity->SetDirty(true);
                }
            }

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
    if (!ImGui::Begin(ICON_MD_PUBLIC "  Level", &m_Active))
    {
        m_Visible = false;

        ImGui::End();

        return;
    }

    m_Visible = true;

    const std::uint64_t id = *static_cast<std::uint32_t*>(Editor::RenderHandler::GetLevelFrameBuffer()->GetColorBuffer()->GetGraphicsIdentifier());

    static auto tabButton = [](const char* text, bool selected)
    {
        bool ret = false;

        if (!selected)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.04f, 0.15f, 0.11f, 1.00f));
        else
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.11f, 0.27f, 0.24f, 1.00f));

        if (ImGui::Button(text, ImVec2(50.f, 0.f)))
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

    ImGui::SameLine();

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 45.f);

    if (PlayHandler::GetGameState() == PlayHandler::EditorGameState::Stopped)
    {
        if (ImGui::Button(ICON_MD_PLAY_ARROW, ImVec2(45.f, 0.f)))
            PlayHandler::Play();
    }
    else
    {
        if (ImGui::Button(ICON_MD_STOP, ImVec2(45.f, 0.f)))
            PlayHandler::Stop();
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

        ImGui::Separator();

        if (ImGui::BeginMenu("Perspective"))
        {
            if (ImGui::MenuItem("2D", nullptr, Editor::LevelEntity::GetPerspective2D()))
            {
                Editor::LevelEntity::SetPerspective2D(true);
            }

            if (ImGui::MenuItem("3D", nullptr, !Editor::LevelEntity::GetPerspective2D()))
            {
                Editor::LevelEntity::SetPerspective2D(false);
            }

            ImGui::EndMenu();
        }

        ImGui::EndPopup();
    }

    ImGui::Spacing();

    const auto avSize = ImGui::GetContentRegionAvail();
    const auto position = ImGui::GetCursorScreenPos();
    const auto renderScale = Editor::RenderHandler::GetLevelRenderingContext()->Size / static_cast<Pine::Vector2f>(Editor::RenderHandler::GetLevelFrameBuffer()->GetSize());

    m_Size = Pine::Vector2i(avSize.x, avSize.y);

    ImGui::Image(id, avSize, ImVec2(0.f, renderScale.y), ImVec2(renderScale.x, 0.f));

    const bool viewportClicked = ImGui::IsItemClicked();
    const bool viewportHovered = ImGui::IsItemHovered();

    if (!m_CaptureMouse)
    {
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        {
            m_CaptureMouse = true;
            m_MouseCapturePosition = Pine::Input::GetCursorPosition();

            Pine::Input::SetCursorMode(Pine::CursorMode::Disabled);
        }
    }
    else
    {
        const auto& io = ImGui::GetIO();

        if (io.MouseWheel != 0.f)
        {
            Editor::LevelEntity::SetSpeedMultiplier(Editor::LevelEntity::GetSpeedMultiplier() + io.MouseWheel * 0.1f);

            m_ShowEntitySpeed = true;
            m_ShowEntitySpeedTime = ImGui::GetTime() + 5.f;
        }

        if (!ImGui::IsMouseDown(ImGuiMouseButton_::ImGuiMouseButton_Right))
        {
            m_CaptureMouse = false;
            Pine::Input::SetCursorMode(Pine::CursorMode::Normal);
        }
    }

    // Allow drag dropping some assets to the viewport such as blueprints, levels and sky boxes.
    if (ImGui::BeginDragDropTarget())
    {
        if (ImGui::AcceptDragDropPayload("Asset", ImGuiDragDropFlags_SourceAllowNullID))
        {
            const auto asset = *static_cast<Pine::Asset**>(ImGui::GetDragDropPayload()->Data);

            if (asset->GetType() == Pine::AssetType::Texture3D)
            {
                const auto currentLevel = Pine::World::GetActiveLevel();
                const auto skybox = dynamic_cast<Pine::Texture3D*>(asset);

                if (currentLevel && skybox)
                {
                    currentLevel->GetLevelSettings().Skybox = skybox;
                }
            }

            if (asset->GetType() == Pine::AssetType::Level)
            {
                if (const auto level = dynamic_cast<Pine::Level*>(asset))
                {
                    Pine::World::SetActiveLevel(level);
                }
            }

            if (asset->GetType() == Pine::AssetType::Model)
            {
                if (const auto model = dynamic_cast<Pine::Model*>(asset))
                {
                    static auto camera = Editor::LevelEntity::Get()->GetComponent<Pine::Camera>();

                    const auto newPosition = camera->GetTransform()->GetPosition() + camera->GetTransform()->GetForward() * 1.25f;
                    const auto newEntity = Pine::Entity::Create(model->GetFileName());

                    newEntity->GetTransform()->SetLocalPosition(newPosition);

                    newEntity->AddComponent<Pine::ModelRenderer>()->SetModel(model);

                    const auto center = (model->GetBoundingBoxMin() + model->GetBoundingBoxMax()) * 0.5f;
                    auto halfSize = (model->GetBoundingBoxMax() - model->GetBoundingBoxMin()) * 0.5f;

                    newEntity->AddComponent<Pine::RigidBody>()->SetRigidBodyType(Pine::RigidBodyType::Static);

                    const auto collider = newEntity->AddComponent<Pine::Collider>();

                    if (halfSize.x < 0.001f)
                        halfSize.x = 0.1f;
                    if (halfSize.y < 0.001f)
                        halfSize.y = 0.1f;
                    if (halfSize.z < 0.001f)
                        halfSize.z = 0.1f;

                    collider->SetPosition(center);
                    collider->SetSize(halfSize);
                }
            }
        }

        ImGui::EndDragDropTarget();
    }

    Editor::LevelEntity::SetCaptureMouse(m_CaptureMouse);

    // Terrain editing takes the left button over: dragging has to sculpt rather than drag a gizmo
    // handle or re-select whatever is under the cursor. 2D is excluded because a top-down
    // orthographic view has no meaningful ray into a height field.
    const bool sculpting = Panels::TerrainTools::IsEditing() && !Editor::LevelEntity::GetPerspective2D();

    if (sculpting)
    {
        UpdateTerrainSculpting(position, viewportHovered, viewportClicked);
    }
    else
    {
        // Leaving edit mode - or starting play - mid-drag still has to close the stroke, or the
        // next one would be recorded as a continuation of it.
        m_SculptDragging = false;

        Editor::TerrainSculpting::EndStroke();

        if (Pine::Rendering::TerrainRenderer::GetBrushOverlay().has_value())
        {
            Pine::Rendering::TerrainRenderer::SetBrushOverlay(std::nullopt);
        }

        RenderTranslationGizmo(position);
    }

    if (Editor::LevelEntity::GetPerspective2D())
    {
        // Handle view port zooming
        if (viewportHovered)
        {
            const auto camera = Editor::RenderHandler::GetLevelRenderingContext()->SceneCamera;
            const float zoomFactor = camera->GetOrthographicSize();
            const float wheelDelta = ImGui::GetIO().MouseWheel * 0.1f * zoomFactor;

            if (wheelDelta != 0.f)
            {
                float newSize = camera->GetOrthographicSize() - wheelDelta;

                if (0.f > newSize)
                    newSize = 0.f;

                camera->SetOrthographicSize(newSize);
            }
        }

        Editor::Gui::Gizmo::Gizmo2D::Render(Pine::Vector2f(position.x, position.y), m_Size);
    }
    else
    {
        Editor::Gui::Gizmo::Gizmo3D::Render({position.x, position.y});
    }

    if (m_ShowEntitySpeed && m_ShowEntitySpeedTime > ImGui::GetTime())
    {
        char buff[64];

        snprintf(buff, sizeof(buff), "Speed: %.1f", Editor::LevelEntity::GetSpeedMultiplier());

        const auto textSize = ImGui::CalcTextSize(buff);

        ImGui::GetWindowDrawList()->AddRectFilled({position.x + 10.f, position.y + 10.f}, {position.x + 20.f + textSize.x, position.y + 20.f + textSize.y}, ImColor(20, 20, 20, 150));
        ImGui::GetWindowDrawList()->AddText({position.x + 15.f, position.y + 15.f}, ImColor(255, 255, 255, 255), buff);
    }

    if (viewportClicked && !ImGuizmo::IsUsing() && !sculpting)
    {
        // Convert the mouse coordinates to the frame buffer position to pass onto
        // the entity selection system.
        auto mousePosition = Pine::Vector2i(ImGui::GetMousePos().x, ImGui::GetMousePos().y);

        // Offset the viewport position
        mousePosition.x -= static_cast<int>(position.x);
        mousePosition.y -= static_cast<int>(position.y);

        // Since our underlying viewport is in 1920x1080, but we down scale that to fit the viewport, we
        // now have to up scale our cursor coordinates back to 1920x1080
        mousePosition = Pine::Vector2f(mousePosition) * Editor::RenderHandler::GetLevelRenderingContext()->Size / Pine::Vector2f(m_Size);

        // Flip Y axis since the frame buffer is flipped
        mousePosition.y = Editor::RenderHandler::GetLevelRenderingContext()->Size.y - mousePosition.y;

        Editor::Gui::EntitySelection::Pick(mousePosition, ImGui::GetIO().KeyCtrl);
    }

    ImGui::End();
}

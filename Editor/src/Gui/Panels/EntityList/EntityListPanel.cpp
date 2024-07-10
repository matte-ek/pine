#include "EntityListPanel.hpp"
#include "Gui/Shared/Selection/Selection.hpp"
#include "IconsMaterialDesign.h"
#include "Pine/Assets/Blueprint/Blueprint.hpp"
#include "Pine/Core/String/String.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Entity/Entity.hpp"
#include "imgui.h"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/World/Components/SpriteRenderer/SpriteRenderer.hpp"

namespace
{
    bool m_Active = true;
    char m_SearchBuffer[128];

    bool m_IsDragDroppingEntity = false;
    bool m_DidDropEntity = false;
    Pine::Entity* m_DroppedEntity = nullptr;

    // Sort of hacky, but this is needed so we can signal that we want to open the EntityListContextMenu
    // popup from inside RenderEntity()
    bool m_OpenEntityContextMenu = false;

    bool m_DidSelectEntity = false;

    void RenderEntity(Pine::Entity* entity)
    {
        // Add the id to the name after '##' (which ImGui won't render) to avoid duplicate id issues,
        // since ImGui builds an id from the widget name.
        const std::string renderText = entity->GetName() + "##" + std::to_string(entity->GetId());

        auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen |
                     ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Framed;

        if (entity->GetChildren().empty())
            flags |= ImGuiTreeNodeFlags_Leaf;

        static const auto runEntityContextWidgets = [](Pine::Entity* entity)
        {
            if (ImGui::IsItemHovered() && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
            {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                {
                    if (Selection::GetSelectedEntities().empty())
                        Selection::Add(entity, true);

                    m_OpenEntityContextMenu = true;
                }
                else
                {
                    if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl))
                    {
                        Selection::Add(entity);
                    }
                    else
                    {
                        Selection::Add(entity, true);
                    }
                }

                m_DidSelectEntity = true;
            }

            if (ImGui::BeginDragDropTarget())
            {
                if (const auto payload = ImGui::AcceptDragDropPayload("Entity", 0))
                {
                    const auto dragDropSourceEntity = *static_cast<Pine::Entity**>(payload->Data);

                    if (dragDropSourceEntity->GetParent() == nullptr)
                    {
                        entity->AddChild(dragDropSourceEntity);
                    }
                    else
                    {
                        dragDropSourceEntity->GetParent()->RemoveChild(dragDropSourceEntity);

                        entity->AddChild(dragDropSourceEntity);
                    }
                }

                ImGui::EndDragDropTarget();
            }

            if (ImGui::BeginDragDropSource())
            {
                ImGui::SetDragDropPayload("Entity", &entity, sizeof(Pine::Entity*), 0);

                ImGui::Text("%s", entity->GetName().c_str());

                m_DroppedEntity = entity;

                ImGui::EndDragDropSource();
            }
        };

        bool restoreFrameColor = false;

        if (!Selection::IsSelected<Pine::Entity>(entity))
        {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.f, 0.f, 0.f, 0.f));

        	restoreFrameColor = true;
        }

        if (ImGui::TreeNodeEx(renderText.c_str(), flags))
        {
            if (restoreFrameColor)
            {
                ImGui::PopStyleColor();
            }

            runEntityContextWidgets(entity);

            for (const auto& child : entity->GetChildren())
            {
                RenderEntity(child);
            }

            ImGui::TreePop();
        }
        else
        {
            if (restoreFrameColor)
            {
                ImGui::PopStyleColor();
            }

            runEntityContextWidgets(entity);
        }
    }

    void RenderEntityMoveSeparator(int entityIndex)
    {
        const auto& mousePos = ImGui::GetMousePos();
        const auto& avSize = ImGui::GetContentRegionAvail();
        const auto& windowDelta = ImGui::GetWindowPos();

        auto cursorPos = ImGui::GetCursorPos();

        cursorPos.x += windowDelta.x;
        cursorPos.y += windowDelta.y;

        if (fabsf((cursorPos.y + 2.f) - mousePos.y) < 4.f)
        {
            ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(cursorPos.x, cursorPos.y),
                                                          ImVec2(cursorPos.x + avSize.x, cursorPos.y + 4.f),
                                                          ImColor(255, 255, 0));

            ImGui::Dummy(ImVec2(0.f, 8.f));

            if (m_DidDropEntity)
            {
                if (entityIndex >= Pine::Entities::GetList().size())
                    entityIndex = static_cast<int>(Pine::Entities::GetList().size()) - 1;

                Pine::Entities::MoveEntity(m_DroppedEntity, entityIndex);
            }
        }
    }

    void UpdateEntityDragDrop()
    {
        bool isDragDroppingEntity = false;

        m_DidDropEntity = false;

        if (const auto payload = ImGui::GetDragDropPayload())
        {
            if (std::string(payload->DataType).find("Entity") != std::string::npos)
            {
                isDragDroppingEntity = true;
            }
        }

        if (isDragDroppingEntity)
        {
            m_IsDragDroppingEntity = true;
        }
        else
        {
            if (m_IsDragDroppingEntity)
            {
                m_DidDropEntity = true;
            }

            m_IsDragDroppingEntity = false;
        }
    }
}

void Panels::EntityList::SetActive(bool value)
{
    m_Active = value;
}

bool Panels::EntityList::GetActive()
{
    return m_Active;
}

void Panels::EntityList::Render()
{
    if (!ImGui::Begin(ICON_MD_SORT " Entity List", &m_Active))
    {
        ImGui::End();

        return;
    }

    m_OpenEntityContextMenu = false;
    m_DidSelectEntity = false;

    ImGui::PushItemWidth(-1.f);
    ImGui::InputTextWithHint("##EntitySearch", ICON_MD_SEARCH " Search...", m_SearchBuffer, IM_ARRAYSIZE(m_SearchBuffer));

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));

    UpdateEntityDragDrop();

    if (m_IsDragDroppingEntity || m_DidDropEntity)
        RenderEntityMoveSeparator(0);

    bool hasSearchQuery = strlen(m_SearchBuffer) > 0;
    std::string searchQuery = hasSearchQuery ? Pine::String::ToLower(m_SearchBuffer) : "";

    for (int i = 0; i < Pine::Entities::GetList().size();i++)
    {
        auto entity = Pine::Entities::GetList()[i];

        if (entity->GetParent() != nullptr)
            continue;
        if (entity->GetTemporary())
            continue;

        if (hasSearchQuery)
        {
            if (Pine::String::ToLower(entity->GetName()).find(searchQuery) == std::string::npos)
                continue;
        }

        RenderEntity(entity);

        if (m_IsDragDroppingEntity || m_DidDropEntity)
            RenderEntityMoveSeparator(i);
    }

    ImGui::PopStyleVar();

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered() && !m_DidSelectEntity)
    {
        Selection::Clear();
    }

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImGui::IsWindowHovered())
    {
        if (!m_OpenEntityContextMenu)
            Selection::Clear();

        ImGui::OpenPopup("EntityListContextMenu");
    }

    if (ImGui::BeginPopup("EntityListContextMenu"))
    {
        if (ImGui::BeginMenu("Create..."))
        {
            if (ImGui::MenuItem("Empty"))
            {
                auto entity = Pine::Entity::Create();

                Selection::Add(entity, true);

                ImGui::CloseCurrentPopup();
            }

            if (ImGui::MenuItem("Model Renderer"))
            {
                auto entity = Pine::Entity::Create();

                entity->AddComponent<Pine::ModelRenderer>();

                Selection::Add(entity, true);

                ImGui::CloseCurrentPopup();
            }

            if (ImGui::MenuItem("Sprite Renderer"))
            {
                auto entity = Pine::Entity::Create();

                entity->AddComponent<Pine::SpriteRenderer>();

                Selection::Add(entity, true);

                ImGui::CloseCurrentPopup();
            }

            if (ImGui::MenuItem("Light"))
            {
                auto entity = Pine::Entity::Create();

                entity->AddComponent<Pine::Light>();

                Selection::Add(entity, true);

                ImGui::CloseCurrentPopup();
            }

            if (ImGui::MenuItem("Camera"))
            {
                auto entity = Pine::Entity::Create();

                entity->AddComponent<Pine::Camera>();

                Selection::Add(entity, true);

                ImGui::CloseCurrentPopup();
            }

            ImGui::EndMenu();
        }

        ImGui::Separator();

        const bool hasSelectedEntity = !Selection::GetSelectedEntities().empty();
        Pine::Entity* primaryEntity = hasSelectedEntity ? Selection::GetSelectedEntities()[0] : nullptr;

        if (ImGui::MenuItem("Delete", nullptr, false, hasSelectedEntity))
        {
            for (auto entity : Selection::GetSelectedEntities())
                entity->Delete();

            Selection::Clear();

            ImGui::CloseCurrentPopup();
        }

        if (ImGui::MenuItem("Rename", nullptr, false, hasSelectedEntity))
        {
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::MenuItem("Duplicate", nullptr, false, hasSelectedEntity))
        {
            auto selectedEntities = Selection::GetSelectedEntities();

            Selection::Clear();

            for (auto selectedEntity : selectedEntities)
            {
                Pine::Blueprint temporaryBlueprint;

                temporaryBlueprint.CreateFromEntity(selectedEntity);

                auto entity = temporaryBlueprint.Spawn();

                Selection::Add(entity);
            }

            ImGui::CloseCurrentPopup();
        }

        if (ImGui::MenuItem("Create child", nullptr, false, hasSelectedEntity) && hasSelectedEntity)
        {
            auto newChild = primaryEntity->CreateChild();

            Selection::Add(newChild, true);

            ImGui::CloseCurrentPopup();
        }

        if (ImGui::MenuItem("Unlink from parent", nullptr, false, hasSelectedEntity && primaryEntity->GetParent() != nullptr) && hasSelectedEntity)
        {
            primaryEntity->GetParent()->RemoveChild(primaryEntity);

            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::End();
}
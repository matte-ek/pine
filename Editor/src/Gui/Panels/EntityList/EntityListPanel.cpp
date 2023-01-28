#include "EntityListPanel.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Entity/Entity.hpp"
#include "imgui.h"

namespace
{
    bool m_Active = true;

    void RenderEntity(Pine::Entity* entity)
    {
        // Add the id to the name after '##' (which ImGui won't render), to avoid duplicate id issues,
        // since ImGui builds an id from the widget name.
        const std::string renderText = entity->GetName() + "##" + std::to_string(entity->GetId());

        auto flags = ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen |
                     ImGuiTreeNodeFlags_SpanFullWidth;

        if (entity->GetChildren().empty())
            flags |= ImGuiTreeNodeFlags_Leaf;

        if (ImGui::TreeNodeEx(renderText.c_str(), flags))
        {
            ImGui::TreePop();
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
    if (!ImGui::Begin("Entity List", &m_Active))
    {
        ImGui::End();

        return;
    }

    for (auto entity : Pine::Entities::GetList())
    {
        if (entity->GetParent() != nullptr)
            continue;
        if (entity->GetTemporary())
            continue;

        RenderEntity(entity);
    }

    ImGui::End();
}

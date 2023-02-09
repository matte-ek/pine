#include "EntityPropertiesRenderer.hpp"
#include "imgui.h"

namespace
{

}

void EntityPropertiesPanel::Render(Pine::Entity* entity)
{
    char nameBuffer[128];

    // General entity properties

    if (entity->GetName().size() < 128)
        strcpy(nameBuffer, entity->GetName().c_str());

    bool isActive = entity->GetActive();
    bool isStatic = entity->GetStatic();

    if (ImGui::Checkbox("##EntityActive", &isActive))
        entity->SetActive(isActive);

    ImGui::SameLine();

    if (ImGui::InputText("##EntityName", nameBuffer, 128))
        entity->SetName(nameBuffer);

    ImGui::SameLine();

    if (ImGui::Checkbox("Static", &isStatic))
        entity->SetStatic(isStatic);

    ImGui::Separator();

    // Components

    // Component creation

    ImGui::Separator();

    if (ImGui::Button("Add new component...", ImVec2(-1.f, 35.f)))
    {

    }
}
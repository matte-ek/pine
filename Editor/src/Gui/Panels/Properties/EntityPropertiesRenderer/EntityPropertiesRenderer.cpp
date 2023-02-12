#include "EntityPropertiesRenderer.hpp"
#include "Gui/Shared/Widgets/Widgets.hpp"
#include "IconsMaterialDesign.h"
#include "imgui.h"

namespace
{
    namespace Components
    {
        void RenderTransform(Pine::Transform* transform)
        {
            auto position = transform->LocalPosition;
            auto rotation = glm::eulerAngles(transform->LocalRotation);
            auto scale = transform->LocalScale;

            if (Widgets::Vector3("Position", position))
            {
                transform->LocalPosition = position;
            }

            if (Widgets::Vector3("Rotation", rotation))
            {
                transform->LocalRotation = rotation;
            }

            if (Widgets::Vector3("Scale", scale))
            {
                transform->LocalScale = scale;
            }
        }

        void Render(Pine::IComponent* component, int index)
        {
            const std::string displayText = std::string(Pine::ComponentTypeToString(component->GetType())) + "##" + std::to_string(index);

            if (ImGui::CollapsingHeader(displayText.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                bool isActive = component->GetActive();

                if (index == 0)
                {
                    // Don't allow the user to disable/remove the Transform component, which should always be first.
                    Widgets::PushDisabled();
                }

                if (ImGui::Checkbox(std::string("Active##" + std::to_string(index)).c_str(), &isActive))
                {
                    component->SetActive(isActive);
                }

                ImGui::SameLine(ImGui::GetWindowWidth() - 32.f);

                if (ImGui::Button(std::string(ICON_MD_DELETE "##" + std::to_string(index)).c_str()))
                {
                    component->GetParent()->RemoveComponent(component);
                    return;
                }

                if (index == 0)
                {
                    // Don't allow the user to disable/remove the Transform component, which should always be first.
                    Widgets::PopDisabled();
                }

                ImGui::Spacing();

                switch (component->GetType())
                {
                case Pine::ComponentType::Transform:
                    RenderTransform(dynamic_cast<Pine::Transform*>(component));
                    break;
                default:
                    break;
                }
            }
        }
    }
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
    int index = 0;
    for (auto component : entity->GetComponents())
    {
        Components::Render(component, index);

        index++;
    }

    // Component creation
    char componentSearchBuffer[64];

    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Add new component...", ImVec2(-1.f, 35.f)))
    {
        strcpy(componentSearchBuffer, "\0");

        ImGui::OpenPopup("AddComponentPopup");

        ImGui::SetKeyboardFocusHere(0);
    }

    if (ImGui::BeginPopup("AddComponentPopup"))
    {
        ImGui::InputTextWithHint("##ComponentSearchBox", ICON_MD_SEARCH " Search...", componentSearchBuffer, 64);



        ImGui::EndPopup();
    }
}
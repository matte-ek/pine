#include "EntityPropertiesRenderer.hpp"

#include <stdexcept>

#include "IconsMaterialDesign.h"
#include "imgui.h"
#include "ComponentPropertiesRenderer/ComponentPropertiesRenderer.hpp"
#include "Gui/Shared/Selection/Selection.hpp"
#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Core/String/String.hpp"
#include "Pine/Game/Game.hpp"
#include "Pine/World/Components/Component/Component.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Components/RigidBody/RigidBody.hpp"

namespace
{
    void HandleTagPicker(Pine::Entity* entity)
    {
        if (ImGui::BeginPopup("EntityTagPopup"))
        {
            ImGui::BeginChild("##EntityTags", ImVec2(300.f, 200.f));

            for (int i = 0; i < 64;i++)
            {
                if (Pine::Game::GetGameProperties().EntityTags[i].empty())
                    continue;

                bool isSelected = entity->GetTags() & (1 << i);
                if (ImGui::Checkbox(Pine::Game::GetGameProperties().EntityTags[i].c_str(), &isSelected))
                {
                    if (isSelected)
                    {
                        entity->SetTags(entity->GetTags() | (1 << i));
                    }
                    else
                    {
                        entity->SetTags(entity->GetTags() & ~(1 << i));
                    }
                }
            }

            ImGui::EndChild();
            ImGui::EndPopup();
        }
    }

    void HandleComponentPicker(Pine::Entity* entity)
    {
        // Component creation
        static char componentSearchBuffer[64];

        static int selectedComponent = 0;
        static std::vector<const char*> componentSearchBox;

        static bool setKeyboardFocus = false;

        static auto componentSearch = [&]() {
            const auto& componentList = Pine::Components::GetComponentTypes();

            componentSearchBox.clear();

            for (const auto& component : componentList)
            {
                const char* componentName = Pine::ComponentTypeToString(component->m_Component->GetType());

                // Do not allow multiple transform components.
                if (component->m_Component->GetType() == Pine::ComponentType::Transform)
                    continue;

                // Native scripts are not allowed to be added through the GUI either.
                if (component->m_Component->GetType() == Pine::ComponentType::NativeScript)
                    continue;

                if (strlen(componentSearchBuffer) == 0)
                {
                    componentSearchBox.push_back(componentName);
                    continue;
                }

                if (Pine::String::ToLower(std::string(componentName)).find(Pine::String::ToLower(std::string(componentSearchBuffer))) == std::string::npos)
                {
                    continue;
                }

                componentSearchBox.push_back(componentName);
            }
        };

        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Add new component...", ImVec2(-1.f, 35.f)))
        {
            strcpy(componentSearchBuffer, "\0");

            componentSearch();

            setKeyboardFocus = true;

            ImGui::OpenPopup("AddComponentPopup");
        }

        if (ImGui::BeginPopup("AddComponentPopup"))
        {
            if (setKeyboardFocus)
            {
                ImGui::SetKeyboardFocusHere();

                selectedComponent = 0;

                setKeyboardFocus = false;
            }

            ImGui::SetKeyboardFocusHere();

            if (ImGui::InputTextWithHint("##ComponentSearchBox", ICON_MD_SEARCH " Search...", componentSearchBuffer, 64))
            {
                componentSearch();
            }

            ImGui::ListBox("##ComponentList", &selectedComponent, componentSearchBox.data(), static_cast<int>(componentSearchBox.size()));

            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                ImGui::CloseCurrentPopup();
            }

            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            {
                selectedComponent--;

                if (0 > selectedComponent)
                    selectedComponent = 0;
            }

            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            {
                selectedComponent++;

                if (selectedComponent >= componentSearchBox.size() - 1)
                    selectedComponent = componentSearchBox.size() - 1;
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::Button("Add", ImVec2(-1.f, 30.f)))
            {
                if (!componentSearchBox.empty())
                {
                    // Find the component type from the selected string
                    const auto& componentList = Pine::Components::GetComponentTypes();
                    const auto selectedComponentStr = componentSearchBox[selectedComponent];

                    auto selectedComponentType = Pine::ComponentType::SpriteRenderer;
                    bool foundComponentType = false;

                    for (const auto& component : componentList)
                    {
                        const char* componentName = Pine::ComponentTypeToString(component->m_Component->GetType());

                        if (strcmp(componentName, selectedComponentStr) == 0)
                        {
                            selectedComponentType = component->m_Component->GetType();
                            foundComponentType = true;
                            break;
                        }
                    }

                    if (!foundComponentType)
                    {
                        throw std::runtime_error("Failed to create component through gui, invalid component type.");
                    }

                    entity->AddComponent(selectedComponentType);
                }

                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
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

    ImGui::SameLine();

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 60.f);

    if (ImGui::Button("Tags " ICON_MD_KEYBOARD_ARROW_DOWN))
        ImGui::OpenPopup("EntityTagPopup");

	ImGui::Separator();

	// Components
	int index = 0;
	for (auto component : entity->GetComponents())
	{
		bool updatedComponentData = ComponentPropertiesRenderer::Render(component, index);

		if (updatedComponentData && Selection::GetSelectedEntities().size() > 1)
		{
		    // Copy component data directly to the other selected entities,
			// this is not ideal in all scenarios, but something is better than nothing
			// when dealing with a lot of entities.
			for (auto selectedEntity : Selection::GetSelectedEntities())
			{
			    if (entity == selectedEntity)
			    {
					continue;
			    }

				auto selectedEntityComponent = selectedEntity->GetComponent(component->GetType());

				if (!selectedEntityComponent)
				{
					continue;
				}


				auto data = component->SaveData();
				selectedEntityComponent->LoadData(data);
			}
		}

	    // Store actions for undo/redo
	    static bool saveComponentAction = false;

	    if (updatedComponentData)
	    {
	        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
	        {
	            saveComponentAction = true;
	        }
	        else
	        {
	            //Actions::RegisterComponentAction(Actions::Modify, component);
	        }
	    }

	    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && saveComponentAction)
	    {
	        saveComponentAction = false;

	        entity->SetDirty(true);

	        //Actions::RegisterComponentAction(Actions::Modify, component);
	    }

		index++;
	}

    HandleTagPicker(entity);
    HandleComponentPicker(entity);
}

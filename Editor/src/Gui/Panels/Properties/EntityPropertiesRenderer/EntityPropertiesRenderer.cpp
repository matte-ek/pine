#include "EntityPropertiesRenderer.hpp"
#include "Gui/Shared/Widgets/Widgets.hpp"
#include "IconsMaterialDesign.h"
#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/Assets/Texture2D/Texture2D.hpp"
#include "Pine/Assets/Tilemap/Tilemap.hpp"
#include "Pine/World/Components/IComponent/IComponent.hpp"
#include "Pine/World/Components/SpriteRenderer/SpriteRenderer.hpp"
#include "Pine/World/Components/TilemapRenderer/TilemapRenderer.hpp"
#include "imgui.h"
#include "Pine/Core/String/String.hpp"
#include <stdexcept>

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

        void RenderSpriteRenderer(Pine::SpriteRenderer* spriteRenderer)
        {
            int scalingMode = static_cast<int>(spriteRenderer->GetScalingMode());
            int order = spriteRenderer->GetOrder();

            auto [newStaticTextureSet, newStaticTexture] = Widgets::AssetPicker("Static Texture", reinterpret_cast<Pine::IAsset*>(spriteRenderer->GetTexture()));

            if (newStaticTextureSet)
                spriteRenderer->SetTexture(dynamic_cast<Pine::Texture2D*>(newStaticTexture));

            if (Widgets::Combobox("Scaling Mode", &scalingMode, "Stretch\0Repeat\0"))
                spriteRenderer->SetScalingMode(static_cast<Pine::SpriteScalingMode>(scalingMode));

            if (Widgets::InputInt("Order", &order))
                spriteRenderer->SetOrder(order);
        }

        void RenderTilemapRenderer(Pine::TilemapRenderer* tilemapRenderer)
        {
            int order = tilemapRenderer->GetOrder();

            auto [newTilemapSet, newTilemap] = Widgets::AssetPicker("Tile map", tilemapRenderer->GetTilemap());

            if (newTilemapSet)
                tilemapRenderer->SetTilemap(dynamic_cast<Pine::Tilemap*>(newTilemap));
        
            if (Widgets::InputInt("Order", &order))
                tilemapRenderer->SetOrder(order);

            if (tilemapRenderer->GetTilemap() != nullptr && tilemapRenderer->GetTilemap()->GetTileset() != nullptr)
            {
                static int selectedTileIndex = 0;
                static bool buildMode = false;

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                auto tileMap = tilemapRenderer->GetTilemap();
                auto tileSet = tileMap->GetTileset();

                Widgets::TilesetAtlas(tileSet, selectedTileIndex);

                ImGui::Checkbox("Build Mode", &buildMode);
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
                    // Don't allow the user to disable/remove the Transform component, which should always be the first one.
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
                    // Don't allow the user to disable/remove the Transform component, which should always be the first one.
                    Widgets::PopDisabled();
                }

                ImGui::Spacing();

                switch (component->GetType())
                {
                case Pine::ComponentType::Transform:
                    RenderTransform(dynamic_cast<Pine::Transform*>(component));
                    break;
                case Pine::ComponentType::SpriteRenderer:
                    RenderSpriteRenderer(dynamic_cast<Pine::SpriteRenderer*>(component));
                    break;
                case Pine::ComponentType::TilemapRenderer:
                    RenderTilemapRenderer(dynamic_cast<Pine::TilemapRenderer*>(component));
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
        strcpy_s(nameBuffer, entity->GetName().c_str());

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

            if (strlen(componentSearchBuffer) == 0)
            {
                componentSearchBox.push_back(componentName);
                continue;
            }

            if (Pine::String::ToLower(std::string(componentName)).find(Pine::String::ToLower(std::string(componentSearchBuffer))) != std::string::npos)
            {
                componentSearchBox.push_back(componentName);
            }
        }
    };

    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Add new component...", ImVec2(-1.f, 35.f)))
    {
        strcpy_s(componentSearchBuffer, "\0");

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

        if (ImGui::InputTextWithHint("##ComponentSearchBox", ICON_MD_SEARCH " Search...", componentSearchBuffer, 64))
        {
            componentSearch();
        }

        ImGui::ListBox("##ComponentList", &selectedComponent, componentSearchBox.data(), static_cast<int>(componentSearchBox.size()));

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
#include "Pine/Core/Math/Math.hpp"
#include "mono/metadata/object.h"
#include <fmt/core.h>
#pragma clang diagnostic push
#pragma ide diagnostic ignored "ArgumentSelectionDefects"

#include "EntityPropertiesRenderer.hpp"
#include "Gui/Shared/Widgets/Widgets.hpp"
#include "IconsMaterialDesign.h"
#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/Assets/Texture2D/Texture2D.hpp"
#include "Pine/Assets/Tilemap/Tilemap.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/World/Components/Collider/Collider.hpp"
#include "Pine/World/Components/IComponent/IComponent.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Components/RigidBody/RigidBody.hpp"
#include "Pine/World/Components/SpriteRenderer/SpriteRenderer.hpp"
#include "Pine/World/Components/TilemapRenderer/TilemapRenderer.hpp"
#include "Pine/World/Components/Script/ScriptComponent.hpp"
#include "Pine/Script/Scripts/ScriptData.hpp"
#include "Pine/Script/Scripts/ScriptField.hpp"
#include "Rendering/RenderHandler.hpp"
#include "imgui.h"
#include "Pine/Core/String/String.hpp"
#include <stdexcept>

namespace
{
    namespace Components
    {
        void RenderTransform(Pine::Transform *transform)
        {
            static bool isApplyingRotation = false;
            static Pine::Vector3f eulerAngles;

            auto position = transform->LocalPosition;
            auto rotation = isApplyingRotation ? eulerAngles : transform->GetEulerAngles();
            auto scale = transform->LocalScale;

            transform->SetDirty();

            if (Widgets::Vector3("Position", position))
            {
                transform->LocalPosition = position;
            }

            if (Widgets::Vector3("Rotation", rotation, 0.5f))
            {
                if (!isApplyingRotation)
                {
                    isApplyingRotation = true;
                }

                eulerAngles = rotation;

                transform->SetEulerAngles(rotation);
            }

            if (Widgets::Vector3("Scale", scale))
            {
                transform->LocalScale = scale;
            }

            if (isApplyingRotation && !ImGui::IsMouseDown(ImGuiMouseButton_::ImGuiMouseButton_Left))
            {
                isApplyingRotation = false;
            }
        }

        void RenderModelRenderer(Pine::ModelRenderer *modelRenderer)
        {
            auto [newModelSet, newModel] = Widgets::AssetPicker("Model", reinterpret_cast<Pine::IAsset *>(modelRenderer->GetModel()), Pine::AssetType::Model);

            if (newModelSet)
                modelRenderer->SetModel(dynamic_cast<Pine::Model *>(newModel));
        }

        void RenderCamera(Pine::Camera *camera)
        {
            int cameraType = static_cast<int>(camera->GetCameraType());
            float nearPlane = camera->GetNearPlane();
            float farPlane = camera->GetFarPlane();
            float fov = camera->GetFieldOfView();
            bool isActiveCamera = RenderHandler::GetGameRenderingContext()->SceneCamera == camera;

            if (Widgets::Combobox("Camera Type", &cameraType, "Perspective\0Orthographic\0"))
            {
                camera->SetCameraType(static_cast<Pine::CameraType>(cameraType));
            }

            if (Widgets::SliderFloat("Near Plane", &nearPlane, 0.01f, 10.f))
            {
                camera->SetNearPlane(nearPlane);
            }

            if (Widgets::SliderFloat("Far Plane", &farPlane, 100.0f, 10000.f))
            {
                camera->SetFarPlane(farPlane);
            }

            if (Widgets::SliderFloat("Field of View", &fov, 10.0f, 180.f))
            {
                camera->SetFieldOfView(fov);
            }

            if (Widgets::Checkbox("Use Camera", &isActiveCamera))
            {
                RenderHandler::GetGameRenderingContext()->SceneCamera = isActiveCamera ? camera : nullptr;
            }
        }

        void RenderLight(Pine::Light *light)
        {
            int lightType = static_cast<int>(light->GetLightType());
            Pine::Vector3f lightColor = light->GetLightColor();

            if (Widgets::Combobox("Light Type", &lightType, "Directional\0Point Light\0Spot Light\0"))
            {
                light->SetLightType(static_cast<Pine::LightType>(lightType));
            }

            if (Widgets::ColorPicker3("Color", lightColor))
            {
                light->SetLightColor(lightColor);
            }
        }

        void RenderCollider(Pine::Collider *collider)
        {
            auto colliderType = static_cast<int>(collider->GetColliderType());
            auto position = collider->GetPosition();
            auto size = collider->GetSize();

            if (Widgets::Combobox("Collider Type", &colliderType, "Box\0Sphere\0Capsule\0Convex Mesh\0Concave Mesh\0Height Field\0"))
            {
                collider->SetColliderType(static_cast<Pine::ColliderType>(colliderType));
            }

            if (colliderType == static_cast<int>(Pine::ColliderType::Box))
            {
                if (Widgets::Vector3("Collider Position", position))
                {
                    collider->SetPosition(position);
                }

                if (Widgets::Vector3("Collider Size", size))
                {
                    collider->SetSize(size);
                }
            } else
            {
                if (Widgets::InputFloat("Collider Radius", &size.x))
                {
                    collider->SetRadius(size.x);
                }

                if (Widgets::InputFloat("Collider Height", &size.y))
                {
                    collider->SetRadius(size.y);
                }
            }
        }

        void RenderRigidBody(Pine::RigidBody *rigidBody)
        {
            auto type = static_cast<int>(rigidBody->GetRigidBodyType());
            auto mass = rigidBody->GetMass();
            auto gravityEnabled = rigidBody->GetGravityEnabled();
            auto &rotationLock = rigidBody->GetRotationLock();

            if (Widgets::Combobox("Rigid Body Type", &type, "Static\0Kinematic\0Dynamic\0"))
            {
                rigidBody->SetRigidBodyType(static_cast<Pine::RigidBodyType>(type));
            }

            if (Widgets::SliderFloat("Mass", &mass, 0, 1000))
            {
                rigidBody->SetMass(mass);
            }

            if (Widgets::Checkbox("Gravity Enabled", &gravityEnabled))
            {
                rigidBody->SetGravityEnabled(gravityEnabled);
            }
        }

        void RenderSpriteRenderer(Pine::SpriteRenderer *spriteRenderer)
        {
            int scalingMode = static_cast<int>(spriteRenderer->GetScalingMode());
            int order = spriteRenderer->GetOrder();

            auto [newStaticTextureSet, newStaticTexture] = Widgets::AssetPicker("Static Texture", reinterpret_cast<Pine::IAsset *>(spriteRenderer->GetTexture()));

            if (newStaticTextureSet)
                spriteRenderer->SetTexture(dynamic_cast<Pine::Texture2D *>(newStaticTexture));

            if (Widgets::Combobox("Scaling Mode", &scalingMode, "Stretch\0Repeat\0"))
                spriteRenderer->SetScalingMode(static_cast<Pine::SpriteScalingMode>(scalingMode));

            if (Widgets::InputInt("Order", &order))
                spriteRenderer->SetOrder(order);
        }

        void RenderTilemapRenderer(Pine::TilemapRenderer *tilemapRenderer)
        {
            int order = tilemapRenderer->GetOrder();

            auto [newTilemapSet, newTilemap] = Widgets::AssetPicker("Tile map", tilemapRenderer->GetTilemap());

            if (newTilemapSet)
                tilemapRenderer->SetTilemap(dynamic_cast<Pine::Tilemap *>(newTilemap));

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

        void RenderScript(Pine::ScriptComponent *scriptComponent)
        {
            auto [newScriptSet, newScript] = Widgets::AssetPicker("Script", scriptComponent->GetScript(), Pine::AssetType::CSharpScript);

            if (newScriptSet)
                scriptComponent->SetScript(dynamic_cast<Pine::CSharpScript *>(newScript));

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (scriptComponent->GetScript() && 
                scriptComponent->GetScript()->GetScriptData() &&
                scriptComponent->GetScript()->GetScriptData()->IsReady && 
                scriptComponent->GetScriptObjectHandle()->Object != nullptr)
            {  
                const auto scriptData = scriptComponent->GetScript()->GetScriptData();
                
                if (scriptData->Fields.empty())
                {
                    ImGui::Text("No fields avaliable.");
                }

                const auto object = mono_gchandle_get_target(scriptComponent->GetScriptObjectHandle()->Handle);

                for (const auto& field : scriptData->Fields)
                {
                    if (field->GetType() == Pine::ScriptFieldType::Float)
                    {
                        float value = field->Get<float>(object);

                        if (Widgets::InputFloat(fmt::format("{} ({})", field->GetName(), Pine::ScriptFieldTypeToString(field->GetType())), &value))
                        {
                            field->Set(object, value);
                        }

                        continue;
                    }

                    if (field->GetType() == Pine::ScriptFieldType::Integer)
                    {
                        int value = field->Get<int>(object);

                        if (Widgets::InputInt(fmt::format("{} ({})", field->GetName(), Pine::ScriptFieldTypeToString(field->GetType())), &value))
                        {
                            field->Set(object, value);
                        }

                        continue;
                    }

                    if (field->GetType() == Pine::ScriptFieldType::Boolean)
                    {
                        bool value = field->Get<bool>(object);

                        if (Widgets::Checkbox(fmt::format("{} ({})", field->GetName(), Pine::ScriptFieldTypeToString(field->GetType())), &value))
                        {
                            field->Set(object, value);
                        }

                        continue;
                    }

                    if (field->GetType() == Pine::ScriptFieldType::Vector3)
                    {
                        Pine::Vector3f value = field->Get<Pine::Vector3f>(object);

                        if (Widgets::Vector3(fmt::format("{} ({})", field->GetName(), Pine::ScriptFieldTypeToString(field->GetType())), value))
                        {
                            field->Set(object, value);
                        }

                        continue;
                    }

                    if (field->GetType() == Pine::ScriptFieldType::Vector2)
                    {
                        Pine::Vector2f value = field->Get<Pine::Vector2f>(object);

                        if (Widgets::Vector2(fmt::format("{} ({})", field->GetName(), Pine::ScriptFieldTypeToString(field->GetType())), value))
                        {
                            field->Set(object, value);
                        }

                        continue;
                    }
                }
            }
        }

        void Render(Pine::IComponent *component, int index)
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

                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16.f);

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
                        RenderTransform(dynamic_cast<Pine::Transform *>(component));
                        break;
                    case Pine::ComponentType::ModelRenderer:
                        RenderModelRenderer(dynamic_cast<Pine::ModelRenderer *>(component));
                        break;
                    case Pine::ComponentType::Camera:
                        RenderCamera(dynamic_cast<Pine::Camera *>(component));
                        break;
                    case Pine::ComponentType::Light:
                        RenderLight(dynamic_cast<Pine::Light *>(component));
                        break;
                    case Pine::ComponentType::Collider:
                        RenderCollider(dynamic_cast<Pine::Collider *>(component));
                        break;
                    case Pine::ComponentType::RigidBody:
                        RenderRigidBody(dynamic_cast<Pine::RigidBody *>(component));
                        break;
                    case Pine::ComponentType::SpriteRenderer:
                        RenderSpriteRenderer(dynamic_cast<Pine::SpriteRenderer *>(component));
                        break;
                    case Pine::ComponentType::TilemapRenderer:
                        RenderTilemapRenderer(dynamic_cast<Pine::TilemapRenderer *>(component));
                        break;
                    case Pine::ComponentType::Script:
                        RenderScript(dynamic_cast<Pine::ScriptComponent *>(component));
                        break;
                    default:
                        break;
                }
            }
        }
    }
}

void EntityPropertiesPanel::Render(Pine::Entity *entity)
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
    for (auto component: entity->GetComponents())
    {
        Components::Render(component, index);

        index++;
    }

    // Component creation
    static char componentSearchBuffer[64];

    static int selectedComponent = 0;
    static std::vector<const char *> componentSearchBox;

    static bool setKeyboardFocus = false;

    static auto componentSearch = [&]() {
        const auto &componentList = Pine::Components::GetComponentTypes();

        componentSearchBox.clear();

        for (const auto &component: componentList)
        {
            const char *componentName = Pine::ComponentTypeToString(component->m_Component->GetType());

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
                const auto &componentList = Pine::Components::GetComponentTypes();
                const auto selectedComponentStr = componentSearchBox[selectedComponent];

                auto selectedComponentType = Pine::ComponentType::SpriteRenderer;
                bool foundComponentType = false;

                for (const auto &component: componentList)
                {
                    const char *componentName = Pine::ComponentTypeToString(component->m_Component->GetType());

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

#pragma clang diagnostic pop
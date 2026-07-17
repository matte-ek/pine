#include "ComponentPropertiesRenderer.hpp"

#include <imgui.h>
#include <fmt/format.h>

#include "IconsMaterialDesign.h"
#include "Gui/Shared/Widgets/Widgets.hpp"
#include "mono/metadata/object.h"
#include "Other/Actions/Actions.hpp"
#include "Pine/Game/Game.hpp"
#include "Pine/Script/Scripts/ScriptData.hpp"
#include "Pine/Script/Scripts/ScriptField.hpp"
#include "Pine/Utilities/Entity/EntityUtilities.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/World/Components/Collider/Collider.hpp"
#include "Pine/World/Components/Collider2D/Collider2D.hpp"
#include "Pine/World/Components/Component/Component.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Components/RigidBody/RigidBody.hpp"
#include "Pine/World/Components/RigidBody2D/RigidBody2D.hpp"
#include "Pine/World/Components/Script/ScriptComponent.hpp"
#include "Pine/World/Components/SpriteRenderer/SpriteRenderer.hpp"
#include "Pine/World/Components/TerrainRenderer/TerrainRendererComponent.hpp"
#include "Pine/World/Components/TilemapRenderer/TilemapRenderer.hpp"
#include "Rendering/RenderHandler.hpp"

namespace
{
    using namespace Editor::Actions;

    void RenderTransform(Pine::Transform* transform)
    {
        static bool isApplyingRotation = false;
        static Pine::Vector3f eulerAngles;

        auto position = transform->GetLocalPosition();
        auto rotation = isApplyingRotation ? eulerAngles : transform->GetEulerAngles();
        auto scale = transform->GetLocalScale();

        transform->SetDirty();

        if (Widgets::Vector3("Position", position))
        {
            CreateComponentCommand updateCmd(transform, CommandType::Update);

            transform->SetLocalPosition(position);
        }

        if (Widgets::Vector3("Rotation", rotation, 0.5f))
        {
            CreateComponentCommand updateCmd(transform, CommandType::Update);

            if (!isApplyingRotation)
            {
                isApplyingRotation = true;
            }

            eulerAngles = rotation;

            transform->SetEulerAngles(rotation);
        }

        if (Widgets::Vector3("Scale", scale))
        {
            CreateComponentCommand updateCmd(transform, CommandType::Update);

            transform->SetLocalScale(scale);
        }

        if (isApplyingRotation && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            isApplyingRotation = false;
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------

    void RenderModelRenderer(Pine::ModelRenderer* modelRenderer)
    {
        auto [newModelSet, newModel] = Widgets::AssetPicker("Model", modelRenderer->GetModel(), Pine::AssetType::Model);

        if (newModelSet)
        {
            CreateComponentCommand updateCmd(modelRenderer, CommandType::Update);

            modelRenderer->SetModel(dynamic_cast<Pine::Model *>(newModel));
        }

        auto [newOverrideMaterialSet, newOverrideMaterial] = Widgets::AssetPicker("Override Material", modelRenderer->GetOverrideMaterial(), Pine::AssetType::Material);

        if (newOverrideMaterialSet)
        {
            CreateComponentCommand updateCmd(modelRenderer, CommandType::Update);

            modelRenderer->SetOverrideMaterial(dynamic_cast<Pine::Material *>(newOverrideMaterial));
        }

        if (modelRenderer->GetParent() != nullptr &&
            modelRenderer->GetModel() != nullptr &&
            modelRenderer->GetModelMeshIndex() == -1)
        {
            if (ImGui::Button("Unpack Model", ImVec2(110.f, 30.f)))
            {
                Pine::Utilities::Entity::UnpackModel(modelRenderer);
            }
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------

    void RenderCamera(Pine::Camera* camera)
    {
        int cameraType = static_cast<int>(camera->GetCameraType());
        float nearPlane = camera->GetNearPlane();
        float farPlane = camera->GetFarPlane();
        float fov = camera->GetFieldOfView();
        bool isActiveCamera = Editor::RenderHandler::GetGameRenderingContext()->SceneCamera == camera;

        if (Widgets::DropDown("Camera Type", &cameraType, "Perspective\0Orthographic\0"))
        {
            CreateComponentCommand updateCmd(camera, CommandType::Update);

            camera->SetCameraType(static_cast<Pine::CameraType>(cameraType));
        }

        if (Widgets::SliderFloat("Near Plane", &nearPlane, 0.01f, 10.f))
        {
            CreateComponentCommand updateCmd(camera, CommandType::Update);

            camera->SetNearPlane(nearPlane);
        }

        if (Widgets::SliderFloat("Far Plane", &farPlane, 100.0f, 10000.f))
        {
            CreateComponentCommand updateCmd(camera, CommandType::Update);

            camera->SetFarPlane(farPlane);
        }

        if (Widgets::SliderFloat("Field of View", &fov, 10.0f, 180.f))
        {
            CreateComponentCommand updateCmd(camera, CommandType::Update);

            camera->SetFieldOfView(fov);
        }

        if (Widgets::Checkbox("Use Camera", &isActiveCamera))
        {
            Editor::RenderHandler::GetGameRenderingContext()->SceneCamera = isActiveCamera ? camera : nullptr;
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------

    void RenderLight(Pine::Light* light)
    {
        int lightType = static_cast<int>(light->GetLightType());
        Pine::Vector3f lightColor = light->GetLightColor();
        Pine::Vector3f lightAttenuation = light->GetLightAttenuation();
        float spotlightRadius = light->GetSpotlightRadius();
        float spotlightCutOff = light->GetSpotlightCutoff();

        if (Widgets::DropDown("Light Type", &lightType, "Directional\0Point Light\0Spot Light\0"))
        {
            CreateComponentCommand updateCmd(light, CommandType::Update);

            light->SetLightType(static_cast<Pine::LightType>(lightType));
        }

        if (Widgets::ColorPicker3("Color", lightColor))
        {
            CreateComponentCommand updateCmd(light, CommandType::Update);

            light->SetLightColor(lightColor);
        }

        if (light->GetLightType() != Pine::LightType::Directional)
        {
            if (Widgets::SliderFloat("Attenuation Constant Factor", &lightAttenuation.x, 0.f, 1.f))
            {
                CreateComponentCommand updateCmd(light, CommandType::Update);

                light->SetLightAttenuation(lightAttenuation);
            }

            if (Widgets::SliderFloat("Attenuation Linear Factor", &lightAttenuation.y, 0.f, 1.f))
            {
                CreateComponentCommand updateCmd(light, CommandType::Update);

                light->SetLightAttenuation(lightAttenuation);
            }

            if (Widgets::SliderFloat("Attenuation Quadratic Factor", &lightAttenuation.z, 0.f, 1.f))
            {
                CreateComponentCommand updateCmd(light, CommandType::Update);

                light->SetLightAttenuation(lightAttenuation);
            }
        }

        if (light->GetLightType() == Pine::LightType::SpotLight)
        {
            if (Widgets::SliderFloat("Spotlight Radius", &spotlightRadius, 0.f, 1.f))
            {
                CreateComponentCommand updateCmd(light, CommandType::Update);

                light->SetSpotlightRadius(spotlightRadius);
            }

            if (Widgets::SliderFloat("Spotlight Cutoff", &spotlightCutOff, 0.f, 1.f))
            {
                CreateComponentCommand updateCmd(light, CommandType::Update);

                light->SetSpotlightCutoff(spotlightCutOff);
            }
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------

    void RenderCollider(Pine::Collider* collider)
    {
        static std::vector<char> layerSelectionBuffer;

        auto colliderType = static_cast<int>(collider->GetColliderType());
        auto position = collider->GetPosition();
        auto size = collider->GetSize();
        auto isTrigger = collider->IsTrigger();
        auto triggerMask = collider->GetTriggerMask();
        auto layer = collider->GetLayer() >> 1;
        auto layerMask = collider->GetLayerMask();

        auto layerIndex = 0;
        while (layer != 0)
        {
            layer = layer >> 1;
            layerIndex++;
        }

        layerSelectionBuffer = {'D', 'e', 'f', 'a', 'u', 'l', 't', '\0'};

        for (const auto& ColliderLayer : Pine::Game::GetGameProperties().ColliderLayers)
        {
            if (ColliderLayer.empty())
            {
                continue;
            }

            for (auto c : ColliderLayer)
            {
                layerSelectionBuffer.push_back(c);
            }

            layerSelectionBuffer.push_back('\0');
        }

        layerSelectionBuffer.push_back('\0');

        if (Widgets::DropDown("Collider Type", &colliderType, "Box\0Sphere\0Capsule\0Convex Mesh\0Concave Mesh\0Height Field\0"))
        {
            CreateComponentCommand updateCmd(collider, CommandType::Update);

            collider->SetColliderType(static_cast<Pine::ColliderType>(colliderType));
        }

        if (colliderType == static_cast<int>(Pine::ColliderType::Box))
        {
            if (Widgets::Vector3("Collider Position", position))
            {
                CreateComponentCommand updateCmd(collider, CommandType::Update);

                collider->SetPosition(position);
            }

            if (Widgets::Vector3("Collider Size", size))
            {
                CreateComponentCommand updateCmd(collider, CommandType::Update);

                collider->SetSize(size);
            }
        }
        else
        {
            if (Widgets::InputFloat("Collider Radius", &size.x))
            {
                CreateComponentCommand updateCmd(collider, CommandType::Update);

                collider->SetRadius(size.x);
            }

            if (Widgets::InputFloat("Collider Height", &size.y))
            {
                CreateComponentCommand updateCmd(collider, CommandType::Update);

                collider->SetHeight(size.y);
            }
        }

        if (Widgets::DropDown("Layer", &layerIndex, layerSelectionBuffer.data()))
        {
            CreateComponentCommand updateCmd(collider, CommandType::Update);

            if (layerIndex == 0)
            {
                collider->SetLayer(Pine::ColliderLayerDefault);
            }
            else
            {
                std::vector<std::string> avLayers;

                for (const auto& colliderLayer : Pine::Game::GetGameProperties().ColliderLayers)
                {
                    if (colliderLayer.empty())
                    {
                        continue;
                    }

                    avLayers.push_back(colliderLayer);
                }

                for (int i = 0; i < 31; i++)
                {
                    if (Pine::Game::GetGameProperties().ColliderLayers[i] == avLayers[layerIndex - 1])
                    {
                        collider->SetLayer(1 << (i + 1));
                    }
                }
            }
        }

        if (Widgets::LayerSelection("Layer Mask", layerMask))
        {
            CreateComponentCommand updateCmd(collider, CommandType::Update);

            collider->SetLayerMask(layerMask);
        }

        if (Widgets::Checkbox("Is Trigger", &isTrigger))
        {
            CreateComponentCommand updateCmd(collider, CommandType::Update);

            collider->SetIsTrigger(isTrigger);
        }

        if (Widgets::LayerSelection("Trigger Mask", triggerMask))
        {
            CreateComponentCommand updateCmd(collider, CommandType::Update);

            collider->SetTriggerMask(triggerMask);
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------

    void RenderRigidBody(Pine::RigidBody* rigidBody)
    {
        auto type = static_cast<int>(rigidBody->GetRigidBodyType());
        auto mass = rigidBody->GetMass();
        auto gravityEnabled = rigidBody->GetGravityEnabled();

        auto positionLock = rigidBody->GetPositionLock();
        auto rotationLock = rigidBody->GetRotationLock();

        auto maxLinearVelocity = rigidBody->GetMaxLinearVelocity();
        auto maxAngularVelocity = rigidBody->GetMaxAngularVelocity();

        if (Widgets::DropDown("Rigid Body Type", &type, "Static\0Kinematic\0Dynamic\0"))
        {
            CreateComponentCommand updateCmd(rigidBody, CommandType::Update);

            rigidBody->SetRigidBodyType(static_cast<Pine::RigidBodyType>(type));
        }

        if (Widgets::SliderFloat("Mass", &mass, 0, 1000))
        {
            CreateComponentCommand updateCmd(rigidBody, CommandType::Update);

            rigidBody->SetMass(mass);
        }

        if (Widgets::CheckboxVector3("Position Lock", positionLock))
        {
            CreateComponentCommand updateCmd(rigidBody, CommandType::Update);

            rigidBody->SetPositionLock(positionLock);
        }

        if (Widgets::CheckboxVector3("Rotation Lock", rotationLock))
        {
            CreateComponentCommand updateCmd(rigidBody, CommandType::Update);

            rigidBody->SetRotationLock(rotationLock);
        }

        if (Widgets::Checkbox("Gravity Enabled", &gravityEnabled))
        {
            CreateComponentCommand updateCmd(rigidBody, CommandType::Update);

            rigidBody->SetGravityEnabled(gravityEnabled);
        }

        if (Widgets::SliderFloat("Max Linear Velocity", &maxLinearVelocity, 0, 1000))
        {
            CreateComponentCommand updateCmd(rigidBody, CommandType::Update);

            rigidBody->SetMaxLinearVelocity(maxLinearVelocity);
        }

        if (Widgets::SliderFloat("Max Angular Velocity", &maxAngularVelocity, 0, 1000))
        {
            CreateComponentCommand updateCmd(rigidBody, CommandType::Update);

            rigidBody->SetMaxAngularVelocity(maxAngularVelocity);
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------

    void RenderSpriteRenderer(Pine::SpriteRenderer* spriteRenderer)
    {
        int scalingMode = static_cast<int>(spriteRenderer->GetScalingMode());
        int order = spriteRenderer->GetOrder();

        auto [newStaticTextureSet, newStaticTexture] = Widgets::AssetPicker("Static Texture", reinterpret_cast<Pine::Asset *>(spriteRenderer->GetTexture()));

        if (newStaticTextureSet)
        {
            CreateComponentCommand updateCmd(spriteRenderer, CommandType::Update);

            spriteRenderer->SetTexture(dynamic_cast<Pine::Texture2D *>(newStaticTexture));
        }

        if (Widgets::DropDown("Scaling Mode", &scalingMode, "Stretch\0Repeat\0"))
        {
            CreateComponentCommand updateCmd(spriteRenderer, CommandType::Update);

            spriteRenderer->SetScalingMode(static_cast<Pine::SpriteScalingMode>(scalingMode));
        }

        if (Widgets::InputInt("Order", &order))
        {
            CreateComponentCommand updateCmd(spriteRenderer, CommandType::Update);

            spriteRenderer->SetOrder(order);
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------

    void RenderTilemapRenderer(Pine::TilemapRenderer* tilemapRenderer)
    {
        int order = tilemapRenderer->GetOrder();

        auto [newTilemapSet, newTilemap] = Widgets::AssetPicker("Tile map", tilemapRenderer->GetTilemap());

        if (newTilemapSet)
        {
            CreateComponentCommand updateCmd(tilemapRenderer, CommandType::Update);

            tilemapRenderer->SetTilemap(dynamic_cast<Pine::Tilemap *>(newTilemap));
        }

        if (Widgets::InputInt("Order", &order))
        {
            CreateComponentCommand updateCmd(tilemapRenderer, CommandType::Update);

            tilemapRenderer->SetOrder(order);
        }

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

    // -----------------------------------------------------------------------------------------------------------------------

    void RenderScript(Pine::ScriptComponent* scriptComponent)
    {
        auto [newScriptSet, newScript] = Widgets::AssetPicker("Script", std::to_string(scriptComponent->GetInternalId()), scriptComponent->GetScript(), Pine::AssetType::CSharpScript);

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
                ImGui::Text("No fields available.");
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

        ImGui::Spacing();
    }

    // -----------------------------------------------------------------------------------------------------------------------

    void RenderCollider2D(Pine::Collider2D* collider)
    {
        auto colliderType = static_cast<int>(collider->GetColliderType());
        auto offset = collider->GetOffset();
        auto size = collider->GetSize();
        auto rotation = collider->GetRotation();

        if (Widgets::DropDown("Collider Type", &colliderType, "Box\0Sprite\0Tilemap"))
        {
            CreateComponentCommand updateCmd(collider, CommandType::Update);

            collider->SetColliderType(static_cast<Pine::Collider2DType>(colliderType));
        }

        if (Widgets::Vector2("Collider Offset", offset))
        {
            CreateComponentCommand updateCmd(collider, CommandType::Update);

            collider->SetOffset(offset);
        }

        if (Widgets::Vector2("Collider Size", size))
        {
            CreateComponentCommand updateCmd(collider, CommandType::Update);

            collider->SetSize(size);
        }

        if (Widgets::InputFloat("Collider Rotation", &rotation))
        {
            CreateComponentCommand updateCmd(collider, CommandType::Update);

            collider->SetRotation(rotation);
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------

    void RenderRigidBody2D(Pine::RigidBody2D* rigidBody2D)
    {
        auto type = static_cast<int>(rigidBody2D->GetRigidBodyType());

        if (Widgets::DropDown("Rigid Body Type", &type, "Static\0Kinematic\0Dynamic\0"))
        {
            CreateComponentCommand updateCmd(rigidBody2D, CommandType::Update);

            rigidBody2D->SetRigidBodyType(static_cast<Pine::RigidBody2DType>(type));
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------

    void RenderTerrainRenderer(Pine::TerrainRendererComponent* terrainRendererComponent)
    {
        auto terrain = terrainRendererComponent->GetTerrain();

        const auto newTerrain = Widgets::AssetPicker("Terrain", terrain, Pine::AssetType::Terrain);

        if (newTerrain.hasResult)
        {
            CreateComponentCommand updateCmd(terrainRendererComponent, CommandType::Update);

            terrainRendererComponent->SetTerrain(dynamic_cast<Pine::Terrain*>(newTerrain.asset));
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------

    void RenderComponent(Pine::Component* component, int index)
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
                CreateComponentCommand createCmd(component, CommandType::Update);

                component->SetActive(isActive);
            }

            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 38.f);

            if (ImGui::Button(std::string(ICON_MD_DELETE "##" + std::to_string(index)).c_str()))
            {
                CreateComponentCommand command(component, CommandType::Delete);

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
                case Pine::ComponentType::Collider2D:
                    RenderCollider2D(dynamic_cast<Pine::Collider2D *>(component));
                    break;
                case Pine::ComponentType::RigidBody2D:
                    RenderRigidBody2D(dynamic_cast<Pine::RigidBody2D *>(component));
                    break;
                case Pine::ComponentType::TerrainRenderer:
                    RenderTerrainRenderer(dynamic_cast<Pine::TerrainRendererComponent *>(component));
                    break;
                default:
                    break;
            }
        }
    }
}

bool ComponentPropertiesRenderer::Render(Pine::Component* component, int index)
{
    RenderComponent(component, index);

    return HasItemUpdated();
}
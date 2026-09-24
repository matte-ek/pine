#include "ComponentPropertiesRenderer.hpp"

#include <imgui.h>
#include <fmt/format.h>

#include <algorithm>
#include <cstring>

#include "IconsMaterialDesign.h"
#include "Gui/Shared/Widgets/Widgets.hpp"
#include "Other/Actions/Actions.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/AudioFile/AudioFile.hpp"
#include "Pine/Game/Game.hpp"
#include "Pine/Script/Scripts/ScriptData.hpp"
#include "Pine/Script/Scripts/ScriptField.hpp"
#include "Pine/Utilities/Entity/EntityUtilities.hpp"
#include "Pine/World/Components/AudioListener/AudioListener.hpp"
#include "Pine/World/Components/AudioSource/AudioSource.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/World/Components/Collider/Collider.hpp"
#include "Pine/World/Components/Collider2D/Collider2D.hpp"
#include "Pine/World/Components/Component/Component.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Components/RigidBody/RigidBody.hpp"
#include "Pine/World/Components/RigidBody2D/RigidBody2D.hpp"
#include "Pine/World/Components/CharacterController/CharacterController.hpp"
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
        float lightIntensity = light->GetLightIntensity();
        float lightRange = light->GetRange();
        bool castShadows = light->GetCastShadows();
        float spotlightOuterAngle = light->GetSpotlightOuterAngle();
        float spotlightInnerAngle = light->GetSpotlightInnerAngle();

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

        // Intensity is unbounded (HDR): values > 1 let the light blow out. The slider covers the
        // span actually worth dragging through and ctrl+click still types anything above it, so
        // there is still no real ceiling. Logarithmic because the windowed inverse-square falloff
        // puts useful values across two decades - a dim fill light sits near 1, while a lamp that
        // has to reach across a room needs tens.
        if (Widgets::SliderFloat("Intensity", &lightIntensity, 0.f, 100.f, true))
        {
            CreateComponentCommand updateCmd(light, CommandType::Update);

            light->SetLightIntensity(lightIntensity);
        }

        // Directional lights are infinitely far away, so a range would be meaningless for them.
        if (light->GetLightType() != Pine::LightType::Directional)
        {
            // The distance at which this light's contribution reaches exactly zero. Logarithmic for
            // the same reason as intensity: the useful values run from a candle (1) through a room
            // (10) to a street (50), and a linear slider would spend most of its travel above the
            // range anything indoors wants.
            if (Widgets::SliderFloat("Range", &lightRange, 0.1f, 100.f, true))
            {
                CreateComponentCommand updateCmd(light, CommandType::Update);

                light->SetRange(lightRange);
            }
        }

        if (Widgets::Checkbox("Cast Shadows", &castShadows))
        {
            CreateComponentCommand updateCmd(light, CommandType::Update);

            light->SetCastShadows(castShadows);
        }

        if (light->GetLightType() == Pine::LightType::SpotLight)
        {
            // Cone half-angles in degrees. The setters clamp inner <= outer, so dragging outer
            // below inner pulls inner down with it rather than inverting the cone.
            if (Widgets::SliderFloat("Spotlight Outer Angle", &spotlightOuterAngle, 1.f, 89.f))
            {
                CreateComponentCommand updateCmd(light, CommandType::Update);

                light->SetSpotlightOuterAngle(spotlightOuterAngle);
            }

            if (Widgets::SliderFloat("Spotlight Inner Angle", &spotlightInnerAngle, 0.f, 89.f))
            {
                CreateComponentCommand updateCmd(light, CommandType::Update);

                light->SetSpotlightInnerAngle(spotlightInnerAngle);
            }
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------

    // The single-layer dropdown, shared by every component that lives in the collision layer space
    // (Collider, CharacterController). Lists "Default" plus the layers named in the project's game
    // properties, and maps the selection back to the single bit the component stores. Returns true
    // and writes 'layer' when the user picked something new.
    bool LayerDropDown(const char* label, std::uint32_t& layer)
    {
        static std::vector<char> layerSelectionBuffer;

        // The stored value is a single bit; turn it back into a dropdown index.
        auto layerBit = layer >> 1;
        auto layerIndex = 0;

        while (layerBit != 0)
        {
            layerBit = layerBit >> 1;
            layerIndex++;
        }

        layerSelectionBuffer = {'D', 'e', 'f', 'a', 'u', 'l', 't', '\0'};

        std::vector<std::string> namedLayers;

        for (const auto& colliderLayer : Pine::Game::GetGameProperties().ColliderLayers)
        {
            if (colliderLayer.empty())
            {
                continue;
            }

            for (auto c : colliderLayer)
            {
                layerSelectionBuffer.push_back(c);
            }

            layerSelectionBuffer.push_back('\0');

            namedLayers.push_back(colliderLayer);
        }

        layerSelectionBuffer.push_back('\0');

        if (!Widgets::DropDown(label, &layerIndex, layerSelectionBuffer.data()))
        {
            return false;
        }

        if (layerIndex == 0)
        {
            layer = Pine::ColliderLayerDefault;

            return true;
        }

        for (int i = 0; i < 31; i++)
        {
            if (Pine::Game::GetGameProperties().ColliderLayers[i] == namedLayers[layerIndex - 1])
            {
                layer = 1 << (i + 1);
            }
        }

        return true;
    }

    // -----------------------------------------------------------------------------------------------------------------------

    void RenderCollider(Pine::Collider* collider)
    {
        auto colliderType = static_cast<int>(collider->GetColliderType());
        auto position = collider->GetPosition();
        auto size = collider->GetSize();
        auto isTrigger = collider->IsTrigger();
        auto triggerMask = collider->GetTriggerMask();
        auto layer = collider->GetLayer();
        auto layerMask = collider->GetLayerMask();

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

        if (LayerDropDown("Layer", layer))
        {
            CreateComponentCommand updateCmd(collider, CommandType::Update);

            collider->SetLayer(layer);
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

        if (Widgets::SliderFloat("Mass", &mass, Pine::RigidBody::MinimumMass, 1000.f))
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

    void RenderCharacterController(Pine::CharacterController* characterController)
    {
        auto radius = characterController->GetRadius();
        auto height = characterController->GetHeight();
        auto slopeLimit = characterController->GetSlopeLimit();
        auto stepOffset = characterController->GetStepOffset();
        auto contactOffset = characterController->GetContactOffset();
        auto gravity = characterController->GetGravity();
        auto layer = characterController->GetLayer();
        auto layerMask = characterController->GetLayerMask();

        if (Widgets::InputFloat("Radius", &radius))
        {
            CreateComponentCommand updateCmd(characterController, CommandType::Update);

            characterController->SetRadius(radius);
        }

        if (Widgets::InputFloat("Height", &height))
        {
            CreateComponentCommand updateCmd(characterController, CommandType::Update);

            characterController->SetHeight(height);
        }

        if (Widgets::SliderFloat("Slope Limit", &slopeLimit, 0.f, 89.f))
        {
            CreateComponentCommand updateCmd(characterController, CommandType::Update);

            characterController->SetSlopeLimit(slopeLimit);
        }

        if (Widgets::InputFloat("Step Offset", &stepOffset))
        {
            CreateComponentCommand updateCmd(characterController, CommandType::Update);

            characterController->SetStepOffset(stepOffset);
        }

        if (Widgets::InputFloat("Contact Offset", &contactOffset))
        {
            CreateComponentCommand updateCmd(characterController, CommandType::Update);

            characterController->SetContactOffset(contactOffset);
        }

        if (Widgets::InputFloat("Gravity", &gravity))
        {
            CreateComponentCommand updateCmd(characterController, CommandType::Update);

            characterController->SetGravity(gravity);
        }

        if (LayerDropDown("Layer", layer))
        {
            CreateComponentCommand updateCmd(characterController, CommandType::Update);

            characterController->SetLayer(layer);
        }

        if (Widgets::LayerSelection("Layer Mask", layerMask))
        {
            CreateComponentCommand updateCmd(characterController, CommandType::Update);

            characterController->SetLayerMask(layerMask);
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

    // Read and write a fixed-size script field as the C++ value the widgets work on. ScriptField
    // hands values over in their stored, type-tagged form, and every type that goes through here
    // is a value type the managed and the native side lay out identically.
    template <typename T>
    bool ReadScriptFieldValue(Pine::ScriptField* field, const Pine::Script::ObjectHandle& object, T& value)
    {
        Pine::ScriptFieldValue storedValue;

        if (!field->ReadValue(object, storedValue) || storedValue.Data.size() != sizeof(T))
        {
            return false;
        }

        std::memcpy(&value, storedValue.Data.data(), sizeof(T));

        return true;
    }

    template <typename T>
    void WriteScriptFieldValue(Pine::ScriptField* field, const Pine::Script::ObjectHandle& object, const T& value)
    {
        Pine::ScriptFieldValue storedValue;

        storedValue.Name = field->GetName();
        storedValue.Type = field->GetType();

        const auto bytes = reinterpret_cast<const std::byte*>(&value);

        storedValue.Data.assign(bytes, bytes + sizeof(T));

        field->WriteValue(object, storedValue);
    }

    // The widget for one script field, without the presentation its attributes ask for - see
    // RenderScriptField below for that. These edit the live managed object directly: the engine
    // takes a copy of its values whenever that object is about to go away (see
    // ScriptComponent::CaptureFieldValues), so nothing here has to save anything itself.
    void RenderScriptFieldWidget(Pine::ScriptField* field, const Pine::Script::ObjectHandle& object, const std::string& widgetId)
    {
        const auto label = fmt::format("{} ({})", field->GetName(), Pine::ScriptFieldTypeToString(field->GetType()));

        switch (field->GetType())
        {
        case Pine::ScriptFieldType::Float:
            {
                float value = 0.f;

                if (!ReadScriptFieldValue(field, object, value))
                    break;

                // A [Range] turns the input field into a slider. Float and Integer are the only
                // two types that can be drawn that way, so it is ignored on the rest.
                const auto edited = field->HasRange()
                    ? Widgets::SliderFloat(label, &value, field->GetRangeMin(), field->GetRangeMax())
                    : Widgets::InputFloat(label, &value);

                if (edited)
                    WriteScriptFieldValue(field, object, value);

                break;
            }
        case Pine::ScriptFieldType::Integer:
            {
                int value = 0;

                if (!ReadScriptFieldValue(field, object, value))
                    break;

                const auto edited = field->HasRange()
                    ? Widgets::SliderInt(label, &value, static_cast<int>(field->GetRangeMin()), static_cast<int>(field->GetRangeMax()))
                    : Widgets::InputInt(label, &value);

                if (edited)
                    WriteScriptFieldValue(field, object, value);

                break;
            }
        case Pine::ScriptFieldType::Boolean:
            {
                bool value = false;

                if (!ReadScriptFieldValue(field, object, value))
                    break;

                if (Widgets::Checkbox(label, &value))
                    WriteScriptFieldValue(field, object, value);

                break;
            }
        case Pine::ScriptFieldType::Vector2:
            {
                Pine::Vector2f value;

                if (!ReadScriptFieldValue(field, object, value))
                    break;

                if (Widgets::Vector2(label, value))
                    WriteScriptFieldValue(field, object, value);

                break;
            }
        case Pine::ScriptFieldType::Vector3:
            {
                Pine::Vector3f value;

                if (!ReadScriptFieldValue(field, object, value))
                    break;

                if (Widgets::Vector3(label, value))
                    WriteScriptFieldValue(field, object, value);

                break;
            }
        case Pine::ScriptFieldType::Vector4:
            {
                Pine::Vector4f value;

                if (!ReadScriptFieldValue(field, object, value))
                    break;

                if (Widgets::Vector4(label, value))
                    WriteScriptFieldValue(field, object, value);

                break;
            }
        case Pine::ScriptFieldType::String:
            {
                Pine::ScriptFieldValue value;

                if (!field->ReadValue(object, value))
                    break;

                // What gets stored has no length limit; this is only how much of it the editor
                // lets you type, in line with the other text fields in the editor.
                char buffer[256] = {};

                std::memcpy(buffer, value.Data.data(), std::min(value.Data.size(), sizeof(buffer) - 1));

                if (Widgets::InputText(label, buffer, sizeof(buffer)))
                {
                    const auto bytes = reinterpret_cast<const std::byte*>(buffer);

                    value.Data.assign(bytes, bytes + std::strlen(buffer));

                    field->WriteValue(object, value);
                }

                break;
            }
        case Pine::ScriptFieldType::Asset:
            {
                Pine::ScriptFieldValue value;

                if (!field->ReadValue(object, value))
                    break;

                Pine::Asset* current = nullptr;

                if (value.Data.size() == sizeof(Pine::UId))
                    current = Pine::Assets::GetAssetByUId(Pine::UId(Pine::ByteSpan(value.Data.data(), value.Data.size())));

                // The picker is restricted to the type the field is declared as, so a Model field
                // cannot be handed a texture that C# would then fail to cast.
                auto [picked, asset] = Widgets::AssetPicker(label, widgetId, current, field->GetAssetType());

                if (picked)
                {
                    value.Data.clear();

                    if (asset != nullptr)
                    {
                        const auto& id = asset->GetUId();
                        const auto bytes = reinterpret_cast<const std::byte*>(&id);

                        value.Data.assign(bytes, bytes + sizeof(Pine::UId));
                    }

                    field->WriteValue(object, value);
                }

                break;
            }
        default:
            // Entity references are reflected so they are visible, but they are not stored yet -
            // saying so beats an empty row the author has to guess about.
            Widgets::Text(label, "Not supported yet");

            break;
        }
    }

    // A field's [Tooltip], shown while the pointer is anywhere over its row.
    //
    // Hovering is tested against the row's rectangle rather than with IsItemHovered, because a
    // Widgets row draws its control into a child window - so the row itself is never the hovered
    // item, and IsItemHovered would answer false wherever the pointer actually is.
    void RenderScriptFieldTooltip(const Pine::ScriptField* field)
    {
        if (field->GetTooltip().empty())
            return;

        if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
            return;

        if (!ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()))
            return;

        ImGui::SetTooltip("%s", field->GetTooltip().c_str());
    }

    // One field of a C# script, drawn the way the attributes on it ask for. Nothing here changes
    // what is stored, so a script may gain or lose an attribute without invalidating a scene.
    void RenderScriptField(Pine::ScriptField* field, const Pine::Script::ObjectHandle& object, const std::string& widgetId)
    {
        // Space first and then the header, so a field carrying both gets a gap and then its title.
        if (field->HasSpace())
            ImGui::Spacing();

        if (!field->GetHeader().empty())
            ImGui::SeparatorText(field->GetHeader().c_str());

        // Grouped so the tooltip has one rectangle covering both the label and the control.
        ImGui::BeginGroup();

        RenderScriptFieldWidget(field, object, widgetId);

        ImGui::EndGroup();

        RenderScriptFieldTooltip(field);
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
            scriptComponent->GetScriptObjectHandle()->IsValid())
        {
            const auto scriptData = scriptComponent->GetScript()->GetScriptData();

            if (scriptData->Fields.empty())
            {
                ImGui::Text("No fields available.");
            }

            const auto& object = *scriptComponent->GetScriptObjectHandle();

            for (const auto& field : scriptData->Fields)
            {
                RenderScriptField(field, object, fmt::format("{}-{}", scriptComponent->GetInternalId(), field->GetName()));
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

    void RenderAudioSource(Pine::AudioSource* audioSource)
    {
        bool playOnStart = audioSource->GetPlayOnStart();
        bool loop = audioSource->GetLoop();
        bool spatial = audioSource->GetSpatial();
        float volume = audioSource->GetVolume();
        float pitch = audioSource->GetPitch();
        float referenceDistance = audioSource->GetReferenceDistance();
        float maxDistance = audioSource->GetMaxDistance();
        float rolloffFactor = audioSource->GetRolloffFactor();

        auto [newAudioFileSet, newAudioFile] = Widgets::AssetPicker("Audio File", audioSource->GetAudioFile(), Pine::AssetType::Audio);

        if (newAudioFileSet)
        {
            CreateComponentCommand updateCmd(audioSource, CommandType::Update);

            audioSource->SetAudioFile(dynamic_cast<Pine::AudioFile*>(newAudioFile));
        }

        if (Widgets::Checkbox("Play On Start", &playOnStart))
        {
            CreateComponentCommand updateCmd(audioSource, CommandType::Update);

            audioSource->SetPlayOnStart(playOnStart);
        }

        if (Widgets::Checkbox("Loop", &loop))
        {
            CreateComponentCommand updateCmd(audioSource, CommandType::Update);

            audioSource->SetLoop(loop);
        }

        // Volume has no hard ceiling - the setter only refuses negatives, and ctrl+click still
        // types anything above the slider - but amplifying a clip past 1 clips the mix, so the
        // drag stops where it stops being a good idea.
        if (Widgets::SliderFloat("Volume", &volume, 0.f, 1.f))
        {
            CreateComponentCommand updateCmd(audioSource, CommandType::Update);

            audioSource->SetVolume(volume);
        }

        // Playback rate as well as pitch, so an octave either way is about as far as a clip stays
        // recognisable. Logarithmic so that half speed and double speed sit the same distance from
        // the middle, which is what makes the two halves of the drag feel alike.
        if (Widgets::SliderFloat("Pitch", &pitch, 0.5f, 2.f, true))
        {
            CreateComponentCommand updateCmd(audioSource, CommandType::Update);

            audioSource->SetPitch(pitch);
        }

        if (Widgets::Checkbox("Spatial", &spatial))
        {
            CreateComponentCommand updateCmd(audioSource, CommandType::Update);

            audioSource->SetSpatial(spatial);
        }

        // A non-spatial source plays on the listener, so distance means nothing to it.
        if (audioSource->GetSpatial())
        {
            // Both distances are logarithmic for the same reason a light's range is: the useful
            // values run from a footstep heard a metre away to ambience carrying across a level,
            // and a linear drag would spend nearly all its travel above anything indoors wants.
            if (Widgets::SliderFloat("Reference Distance", &referenceDistance, 0.1f, 100.f, true))
            {
                CreateComponentCommand updateCmd(audioSource, CommandType::Update);

                audioSource->SetReferenceDistance(referenceDistance);
            }

            // The setter holds this at or above the reference distance, so dragging it below pins
            // it there rather than inverting the fade.
            if (Widgets::SliderFloat("Max Distance", &maxDistance, 0.1f, 500.f, true))
            {
                CreateComponentCommand updateCmd(audioSource, CommandType::Update);

                audioSource->SetMaxDistance(maxDistance);
            }

            if (Widgets::SliderFloat("Rolloff Factor", &rolloffFactor, 0.f, 10.f))
            {
                CreateComponentCommand updateCmd(audioSource, CommandType::Update);

                audioSource->SetRolloffFactor(rolloffFactor);
            }
        }

        // Read-only, and only worth anything in play mode - but it is the one part of a source you
        // cannot work out by looking at the scene, and the one you want while chasing a sound that
        // will not start.
        const char* playbackState = "Stopped";

        switch (audioSource->GetPlaybackState())
        {
            case Pine::Audio::PlaybackState::Playing:
                playbackState = "Playing";
                break;
            case Pine::Audio::PlaybackState::Paused:
                playbackState = "Paused";
                break;
            default:
                break;
        }

        ImGui::Spacing();

        Widgets::Text("State", fmt::format("{} ({:.2f}s)", playbackState, audioSource->GetPlaybackPosition()));
    }

    // -----------------------------------------------------------------------------------------------------------------------

    void RenderAudioListener(Pine::AudioListener* audioListener)
    {
        float volume = audioListener->GetVolume();

        if (Widgets::SliderFloat("Volume", &volume, 0.f, 1.f))
        {
            CreateComponentCommand updateCmd(audioListener, CommandType::Update);

            audioListener->SetVolume(volume);
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
                case Pine::ComponentType::CharacterController:
                    RenderCharacterController(dynamic_cast<Pine::CharacterController *>(component));
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
                case Pine::ComponentType::AudioSource:
                    RenderAudioSource(dynamic_cast<Pine::AudioSource *>(component));
                    break;
                case Pine::ComponentType::AudioListener:
                    RenderAudioListener(dynamic_cast<Pine::AudioListener *>(component));
                    break;
                default:
                    break;
            }
        }
    }
}

bool ComponentPropertiesRenderer::Render(Pine::Component* component, int index)
{
    // The updated flag is frame-global and anything that ran earlier in the frame may have set it - the
    // viewport gizmo does exactly that while dragging. Clear it first so the return value means "this
    // component's own widgets changed something", which is what the caller acts on.
    ClearItemUpdated();

    RenderComponent(component, index);

    return HasItemUpdated();
}
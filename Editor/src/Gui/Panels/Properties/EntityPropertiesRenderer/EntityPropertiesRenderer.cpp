#include "EntityPropertiesRenderer.hpp"

#include <stdexcept>
#include <fmt/core.h>

#include "IconsMaterialDesign.h"
#include "imgui.h"
#include "Gui/Shared/Selection/Selection.hpp"
#include "Gui/Shared/Widgets/Widgets.hpp"
#include "mono/metadata/object.h"
#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/Assets/Texture2D/Texture2D.hpp"
#include "Pine/Assets/Tilemap/Tilemap.hpp"
#include "Pine/Core/Math/Math.hpp"
#include "Pine/Core/String/String.hpp"
#include "Pine/Script/Scripts/ScriptData.hpp"
#include "Pine/Script/Scripts/ScriptField.hpp"
#include "Pine/Utilities/Entity/EntityUtilities.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/World/Components/Collider/Collider.hpp"
#include "Pine/World/Components/Collider2D/Collider2D.hpp"
#include "Pine/World/Components/IComponent/IComponent.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Components/RigidBody/RigidBody.hpp"
#include "Pine/World/Components/RigidBody2D/RigidBody2D.hpp"
#include "Pine/World/Components/Script/ScriptComponent.hpp"
#include "Pine/World/Components/SpriteRenderer/SpriteRenderer.hpp"
#include "Pine/World/Components/TilemapRenderer/TilemapRenderer.hpp"
#include "Rendering/RenderHandler.hpp"

namespace
{
	namespace Components
	{
		// Keeping this as a "global" variable since I'm too lazy to pass it around
		// to each RenderBlahBlah function.
		bool m_UpdatedComponentData = false;

		void RenderTransform(Pine::Transform* transform)
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
				m_UpdatedComponentData = true;
			}

			if (Widgets::Vector3("Rotation", rotation, 0.5f))
			{
				if (!isApplyingRotation)
				{
					isApplyingRotation = true;
				}

				eulerAngles = rotation;

				transform->SetEulerAngles(rotation);

				m_UpdatedComponentData = true;
			}

			if (Widgets::Vector3("Scale", scale))
			{
				transform->LocalScale = scale;
				m_UpdatedComponentData = true;
			}

			if (isApplyingRotation && !ImGui::IsMouseDown(ImGuiMouseButton_::ImGuiMouseButton_Left))
			{
				isApplyingRotation = false;
			}
		}

		void RenderModelRenderer(Pine::ModelRenderer* modelRenderer)
		{
			auto [newModelSet, newModel] = Widgets::AssetPicker("Model", modelRenderer->GetModel(), Pine::AssetType::Model);

		    if (newModelSet)
			{
				modelRenderer->SetModel(dynamic_cast<Pine::Model*>(newModel));
				m_UpdatedComponentData = true;
			}

			if (modelRenderer->GetParent() != nullptr)
			{
				if (ImGui::Button("Unpack Model"))
				{
					Pine::Utilities::Entity::UnpackModel(modelRenderer);
				}
			}
		}

		void RenderCamera(Pine::Camera* camera)
		{
			int cameraType = static_cast<int>(camera->GetCameraType());
			float nearPlane = camera->GetNearPlane();
			float farPlane = camera->GetFarPlane();
			float fov = camera->GetFieldOfView();
			bool isActiveCamera = RenderHandler::GetGameRenderingContext()->SceneCamera == camera;

			if (Widgets::Combobox("Camera Type", &cameraType, "Perspective\0Orthographic\0"))
			{
				camera->SetCameraType(static_cast<Pine::CameraType>(cameraType));
				m_UpdatedComponentData = true;
			}

			if (Widgets::SliderFloat("Near Plane", &nearPlane, 0.01f, 10.f))
			{
				camera->SetNearPlane(nearPlane);
				m_UpdatedComponentData = true;
			}

			if (Widgets::SliderFloat("Far Plane", &farPlane, 100.0f, 10000.f))
			{
				camera->SetFarPlane(farPlane);
				m_UpdatedComponentData = true;
			}

			if (Widgets::SliderFloat("Field of View", &fov, 10.0f, 180.f))
			{
				camera->SetFieldOfView(fov);
				m_UpdatedComponentData = true;
			}

			if (Widgets::Checkbox("Use Camera", &isActiveCamera))
			{
				RenderHandler::GetGameRenderingContext()->SceneCamera = isActiveCamera ? camera : nullptr;
			}
		}

		void RenderLight(Pine::Light* light)
		{
			int lightType = static_cast<int>(light->GetLightType());
			Pine::Vector3f lightColor = light->GetLightColor();

			if (Widgets::Combobox("Light Type", &lightType, "Directional\0Point Light\0Spot Light\0"))
			{
				light->SetLightType(static_cast<Pine::LightType>(lightType));
				m_UpdatedComponentData = true;
			}

			if (Widgets::ColorPicker3("Color", lightColor))
			{
				light->SetLightColor(lightColor);
				m_UpdatedComponentData = true;
			}
		}

		void RenderCollider(Pine::Collider* collider)
		{
			auto colliderType = static_cast<int>(collider->GetColliderType());
			auto position = collider->GetPosition();
			auto size = collider->GetSize();

			if (Widgets::Combobox("Collider Type", &colliderType, "Box\0Sphere\0Capsule\0Convex Mesh\0Concave Mesh\0Height Field\0"))
			{
				collider->SetColliderType(static_cast<Pine::ColliderType>(colliderType));
				m_UpdatedComponentData = true;
			}

			if (colliderType == static_cast<int>(Pine::ColliderType::Box))
			{
				if (Widgets::Vector3("Collider Position", position))
				{
					collider->SetPosition(position);
					m_UpdatedComponentData = true;
				}

				if (Widgets::Vector3("Collider Size", size))
				{
					collider->SetSize(size);
					m_UpdatedComponentData = true;
				}
			}
			else
			{
				if (Widgets::InputFloat("Collider Radius", &size.x))
				{
					collider->SetRadius(size.x);
					m_UpdatedComponentData = true;
				}

				if (Widgets::InputFloat("Collider Height", &size.y))
				{
					collider->SetRadius(size.y);
					m_UpdatedComponentData = true;
				}
			}
		}

		void RenderRigidBody(Pine::RigidBody* rigidBody)
		{
			auto type = static_cast<int>(rigidBody->GetRigidBodyType());
			auto mass = rigidBody->GetMass();
			auto gravityEnabled = rigidBody->GetGravityEnabled();

			if (Widgets::Combobox("Rigid Body Type", &type, "Static\0Kinematic\0Dynamic\0"))
			{
				rigidBody->SetRigidBodyType(static_cast<Pine::RigidBodyType>(type));
				m_UpdatedComponentData = true;
			}

			if (Widgets::SliderFloat("Mass", &mass, 0, 1000))
			{
				rigidBody->SetMass(mass);
				m_UpdatedComponentData = true;
			}

			if (Widgets::Checkbox("Gravity Enabled", &gravityEnabled))
			{
				rigidBody->SetGravityEnabled(gravityEnabled);
				m_UpdatedComponentData = true;
			}
		}

		void RenderSpriteRenderer(Pine::SpriteRenderer* spriteRenderer)
		{
			int scalingMode = static_cast<int>(spriteRenderer->GetScalingMode());
			int order = spriteRenderer->GetOrder();

			auto [newStaticTextureSet, newStaticTexture] = Widgets::AssetPicker("Static Texture", reinterpret_cast<Pine::IAsset*>(spriteRenderer->GetTexture()));

			if (newStaticTextureSet)
			{
				spriteRenderer->SetTexture(dynamic_cast<Pine::Texture2D*>(newStaticTexture));
				m_UpdatedComponentData = true;
			}

			if (Widgets::Combobox("Scaling Mode", &scalingMode, "Stretch\0Repeat\0"))
			{
				spriteRenderer->SetScalingMode(static_cast<Pine::SpriteScalingMode>(scalingMode));
				m_UpdatedComponentData = true;
			}

			if (Widgets::InputInt("Order", &order))
			{
				spriteRenderer->SetOrder(order);
				m_UpdatedComponentData = true;
			}
		}

		void RenderTilemapRenderer(Pine::TilemapRenderer* tilemapRenderer)
		{
			int order = tilemapRenderer->GetOrder();

			auto [newTilemapSet, newTilemap] = Widgets::AssetPicker("Tile map", tilemapRenderer->GetTilemap());

			if (newTilemapSet)
			{
				tilemapRenderer->SetTilemap(dynamic_cast<Pine::Tilemap*>(newTilemap));
				m_UpdatedComponentData = true;
			}

			if (Widgets::InputInt("Order", &order))
			{
				tilemapRenderer->SetOrder(order);
				m_UpdatedComponentData = true;
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

		void RenderScript(Pine::ScriptComponent* scriptComponent)
		{
			auto [newScriptSet, newScript] = Widgets::AssetPicker("Script", scriptComponent->GetScript(), Pine::AssetType::CSharpScript);

			if (newScriptSet)
				scriptComponent->SetScript(dynamic_cast<Pine::CSharpScript*>(newScript));

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
		}

		void RenderCollider2D(Pine::Collider2D* collider)
		{
			auto colliderType = static_cast<int>(collider->GetColliderType());
			auto offset = collider->GetOffset();
			auto size = collider->GetSize();
			auto rotation = collider->GetRotation();

			if (Widgets::Combobox("Collider Type", &colliderType, "Box\0Sprite\0Tilemap"))
			{
				collider->SetColliderType(static_cast<Pine::Collider2DType>(colliderType));
				m_UpdatedComponentData = true;
			}

			if (Widgets::Vector2("Collider Offset", offset))
			{
				collider->SetOffset(offset);
				m_UpdatedComponentData = true;
			}

			if (Widgets::Vector2("Collider Size", size))
			{
				collider->SetSize(size);
				m_UpdatedComponentData = true;
			}

			if (Widgets::InputFloat("Collider Rotation", &rotation))
			{
				collider->SetRotation(rotation);
				m_UpdatedComponentData = true;
			}
		}

		void RenderRigidBody2D(Pine::RigidBody2D* rigidBody2D)
		{
			auto type = static_cast<int>(rigidBody2D->GetRigidBodyType());

			if (Widgets::Combobox("Rigid Body Type", &type, "Static\0Kinematic\0Dynamic\0"))
			{
				rigidBody2D->SetRigidBodyType(static_cast<Pine::RigidBody2DType>(type));
				m_UpdatedComponentData = true;
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

				m_UpdatedComponentData = false;

				switch (component->GetType())
				{
				case Pine::ComponentType::Transform:
					RenderTransform(dynamic_cast<Pine::Transform*>(component));
					break;
				case Pine::ComponentType::ModelRenderer:
					RenderModelRenderer(dynamic_cast<Pine::ModelRenderer*>(component));
					break;
				case Pine::ComponentType::Camera:
					RenderCamera(dynamic_cast<Pine::Camera*>(component));
					break;
				case Pine::ComponentType::Light:
					RenderLight(dynamic_cast<Pine::Light*>(component));
					break;
				case Pine::ComponentType::Collider:
					RenderCollider(dynamic_cast<Pine::Collider*>(component));
					break;
				case Pine::ComponentType::RigidBody:
					RenderRigidBody(dynamic_cast<Pine::RigidBody*>(component));
					break;
				case Pine::ComponentType::SpriteRenderer:
					RenderSpriteRenderer(dynamic_cast<Pine::SpriteRenderer*>(component));
					break;
				case Pine::ComponentType::TilemapRenderer:
					RenderTilemapRenderer(dynamic_cast<Pine::TilemapRenderer*>(component));
					break;
				case Pine::ComponentType::Script:
					RenderScript(dynamic_cast<Pine::ScriptComponent*>(component));
					break;
				case Pine::ComponentType::Collider2D:
					RenderCollider2D(dynamic_cast<Pine::Collider2D*>(component));
					break;
				case Pine::ComponentType::RigidBody2D:
					RenderRigidBody2D(dynamic_cast<Pine::RigidBody2D*>(component));
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

		if (Components::m_UpdatedComponentData && Selection::GetSelectedEntities().size() > 1)
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

				nlohmann::json j;

				component->SaveData(j);
				selectedEntityComponent->LoadData(j);
			}
		}

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

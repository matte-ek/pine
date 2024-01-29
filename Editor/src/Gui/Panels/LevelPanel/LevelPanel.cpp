#include "LevelPanel.hpp"
#include "imgui.h"
#include "IconsMaterialDesign.h"
#include "Pine/World/World.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/Assets/Texture3D/Texture3D.hpp"
#include "Gui/Shared/Widgets/Widgets.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"

namespace
{
    bool m_Active = true;
}

void Panels::LevelPanel::SetActive(bool value)
{
    m_Active = value;
}

bool Panels::LevelPanel::GetActive()
{
    return m_Active;
}

void Panels::LevelPanel::Render()
{
    auto currentLevel = Pine::World::GetActiveLevel();

    if (!currentLevel)
        return;

    if (ImGui::Begin(ICON_MD_PUBLIC " Level Properties", &m_Active))
    {
        const auto camera = Pine::RenderManager::GetPrimaryRenderingContext()->SceneCamera;
        const auto cameraParent = camera != nullptr ? camera->GetParent() : nullptr;

        const auto newSkybox = Widgets::AssetPicker("Skybox", currentLevel->GetLevelSettings().Skybox.Get(), Pine::AssetType::Texture3D);
        const auto newCameraEntity = Widgets::EntityPicker("Camera", cameraParent);

        if (newSkybox.hasResult)
        {
            currentLevel->GetLevelSettings().Skybox = dynamic_cast<Pine::Texture3D*>(newSkybox.asset);
        }

        if (newCameraEntity.hasResult)
        {
            if (newCameraEntity.entity != nullptr)
            {
                Pine::RenderManager::GetPrimaryRenderingContext()->SceneCamera = newCameraEntity.entity->GetComponent<Pine::Camera>();
            }
            else
            {
                Pine::RenderManager::GetPrimaryRenderingContext()->SceneCamera = nullptr;
            }
        }
    }
    ImGui::End();
}

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

    if (ImGui::Begin(ICON_MD_PUBLIC "  Level Properties", &m_Active))
    {
        const auto camera = Pine::RenderManager::GetPrimaryRenderingContext()->SceneCamera;
        const auto cameraParent = camera != nullptr ? camera->GetParent() : nullptr;

        const auto newSkybox = Widgets::AssetPicker("Skybox", currentLevel->GetLevelSettings().Skybox.Get(), Pine::AssetType::Texture3D);
        const auto newCameraEntity = Widgets::EntityPicker("Camera", cameraParent);

        Widgets::ColorPicker3("Ambient Color", currentLevel->GetLevelSettings().AmbientColor);
        Widgets::ColorPicker4("Fog Color", currentLevel->GetLevelSettings().FogColor);

        Widgets::SliderFloat("Fog Intensity", &currentLevel->GetLevelSettings().FogIntensity, 0.0f, 1.0f);
        Widgets::SliderFloat("Fog Distance", &currentLevel->GetLevelSettings().FogDistance, 1.0f, 250.0f);

        Widgets::SliderFloat("Exposure", &currentLevel->GetLevelSettings().Exposure, 0.0f, 8.0f);

        Widgets::SliderFloat("Bloom Threshold", &currentLevel->GetLevelSettings().BloomThreshold, 0.0f, 5.0f);
        Widgets::SliderFloat("Bloom Intensity", &currentLevel->GetLevelSettings().BloomIntensity, 0.0f, 2.0f);

        Widgets::SliderFloat("Grain Strength", &currentLevel->GetLevelSettings().GrainStrength, 0.0f, 0.3f);
        Widgets::SliderFloat("Vignette Strength", &currentLevel->GetLevelSettings().VignetteStrength, 0.0f, 1.0f);

        Widgets::SliderFloat("Wind Direction", &currentLevel->GetLevelSettings().WindDirection, 0.0f, 360.0f);
        Widgets::SliderFloat("Wind Strength", &currentLevel->GetLevelSettings().WindStrength, 0.0f, 1.0f);
        Widgets::SliderFloat("Wind Speed", &currentLevel->GetLevelSettings().WindSpeed, 0.0f, 3.0f);

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

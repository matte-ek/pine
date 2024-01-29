#include "Level.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"
#include "Pine/World/World.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"

Pine::Level::Level()
{
    m_Type = AssetType::Level;
    m_LoadMode = AssetLoadMode::MultiThread;
}

void Pine::Level::CreateFromWorld()
{
    auto primaryRenderingContext = Pine::RenderManager::GetPrimaryRenderingContext();
    const auto currentCameraEntity = primaryRenderingContext->SceneCamera != nullptr ? primaryRenderingContext->SceneCamera->GetParent() : nullptr;

    ClearBlueprints();

    for (const auto& entity : Entities::GetList())
    {
        // Ignore children as we take care of those when processing their parents.
        if (entity->GetParent() != nullptr)
            continue;

        // See comment for m_Temporary
        if (entity->GetTemporary())
            continue;

        if (currentCameraEntity == entity)
        {
            m_LevelSettings.HasCamera = true;
            m_LevelSettings.CameraEntity = m_Blueprints.size();
        }

        auto blueprint = new Blueprint();

        blueprint->CreateFromEntity(entity);

        m_Blueprints.push_back(blueprint);
    }

    m_LevelSettings.Skybox = primaryRenderingContext->Skybox;

    m_HasBeenModified = true;
}

void Pine::Level::Load()
{
    auto primaryRenderingContext = Pine::RenderManager::GetPrimaryRenderingContext();

    Entities::DeleteAll();

    for (int i = 0; i < m_Blueprints.size();i++)
    {
        auto entity = m_Blueprints[i]->Spawn();

        if (m_LevelSettings.HasCamera && m_LevelSettings.CameraEntity == i)
        {
            primaryRenderingContext->SceneCamera = entity->GetComponent<Pine::Camera>();
        }
    }

    if (m_LevelSettings.Skybox.Get())
    {
        primaryRenderingContext->Skybox = m_LevelSettings.Skybox.Get();
    }

    World::SetActiveLevel(this, true);
}

void Pine::Level::ClearBlueprints()
{
    for (auto blueprint : m_Blueprints)
    {
        blueprint->Dispose();

        delete blueprint;
    }

    m_Blueprints.clear();
    m_HasBeenModified = true;
}

std::size_t Pine::Level::GetBlueprintCount() const
{
    return m_Blueprints.size();
}

Pine::LevelSettings& Pine::Level::GetLevelSettings()
{
    return m_LevelSettings;
}

bool Pine::Level::LoadFromFile(AssetLoadStage stage)
{
    const auto json = Serialization::LoadFromFile(m_FilePath);

    if (!json.has_value())
    {
        return false;
    }

    const auto& j = json.value();

    if (j.contains("entities"))
    {
        for (const auto& blueprintJson : j["entities"])
        {
            auto blueprint = new Blueprint();

            blueprint->FromJson(blueprintJson);

            m_Blueprints.push_back(blueprint);
        }
    }

    if (j.contains("settings"))
    {
        Serialization::LoadValue(j["settings"], "camera", m_LevelSettings.CameraEntity);
        Serialization::LoadAsset(j["settings"], "skybox", m_LevelSettings.Skybox);

        if (j["settings"].contains("camera"))
        {
            m_LevelSettings.HasCamera = true;
        }
    }

    m_State = AssetState::Loaded;

    return true;
}

bool Pine::Level::SaveToFile()
{
    nlohmann::json j;

    for (auto bp : m_Blueprints)
    {
        j["entities"].push_back(bp->ToJson());
    }

    if (m_LevelSettings.HasCamera)
        j["settings"]["camera"] = m_LevelSettings.CameraEntity;

    j["settings"]["skybox"] = Serialization::StoreAsset(m_LevelSettings.Skybox);

    Serialization::SaveToFile(m_FilePath, j);

    return true;
}

void Pine::Level::Dispose()
{
    ClearBlueprints();
}
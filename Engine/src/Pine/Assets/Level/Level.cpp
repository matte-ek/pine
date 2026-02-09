#include "Level.hpp"
#include "../../Core/Serialization/Json/SerializationJson.hpp"
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

    if (currentCameraEntity != nullptr)
    {
        int id = 0;

        for (const auto& entity : Entities::GetList())
        {
            id++;

            if (entity->GetTemporary())
            {
                continue;
            }

            if (entity == currentCameraEntity)
            {
                m_LevelSettings.CameraEntity = id;
                m_LevelSettings.HasCamera = true;
                break;
            }
        }
    }

    for (const auto& entity : Entities::GetList())
    {
        // Ignore children as we take care of those when processing their parents.
        if (entity->GetParent() != nullptr)
            continue;

        // See comment for m_Temporary
        if (entity->GetTemporary())
            continue;

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

    const auto entityOffset = Entities::GetList().size();

    for (const auto& blueprint : m_Blueprints)
    {
        blueprint->Spawn();
    }

    if (m_LevelSettings.HasCamera)
    {
        const auto& entityList = Entities::GetList();
        const auto entityCameraIndex = m_LevelSettings.CameraEntity - entityOffset;

        if (entityCameraIndex < entityList.size())
        {
            primaryRenderingContext->SceneCamera = entityList[entityCameraIndex]->GetComponent<Pine::Camera>();
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
    const auto json = SerializationJson::LoadFromFile(m_FilePath);

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
        SerializationJson::LoadValue(j["settings"], "camera", m_LevelSettings.CameraEntity);
        SerializationJson::LoadAsset(j["settings"], "skybox", m_LevelSettings.Skybox);
        SerializationJson::LoadVector3(j["settings"], "ambientColor", m_LevelSettings.AmbientColor);
        SerializationJson::LoadVector4(j["settings"], "fogColor", m_LevelSettings.FogColor);
        SerializationJson::LoadValue(j["settings"], "fogDistance", m_LevelSettings.FogDistance);
        SerializationJson::LoadValue(j["settings"], "fogIntensity", m_LevelSettings.FogIntensity);

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

    j["settings"]["skybox"] = SerializationJson::StoreAsset(m_LevelSettings.Skybox);
    j["settings"]["ambientColor"] = SerializationJson::StoreVector3(m_LevelSettings.AmbientColor);
    j["settings"]["fogColor"] = SerializationJson::StoreVector4(m_LevelSettings.FogColor);
    j["settings"]["fogDistance"] = m_LevelSettings.FogDistance;
    j["settings"]["fogIntensity"] = m_LevelSettings.FogIntensity;

    SerializationJson::SaveToFile(m_FilePath, j);

    return true;
}

void Pine::Level::Dispose()
{
    ClearBlueprints();
}
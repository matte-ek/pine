#include "Level.hpp"
#include "../../Core/Serialization/Json/SerializationJson.hpp"
#include "Pine/Core/File/File.hpp"
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
    auto primaryRenderingContext = RenderManager::GetPrimaryRenderingContext();
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
        {
            continue;
        }

        // See comment for m_Temporary
        if (entity->GetTemporary())
        {
            continue;
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
    const auto fileByteSpan = File::ReadCompressed(m_FilePath);

    if (!fileByteSpan.data)
    {
        return false;
    }

    LevelSerializer levelSerializer;

    levelSerializer.Read(fileByteSpan);

    for (int i = 0; i < levelSerializer.Blueprints.GetDataCount();i++)
    {
        auto blueprint = new Blueprint();

        blueprint->FromByteSpan(levelSerializer.Blueprints.GetData(i));

        m_Blueprints.push_back(blueprint);
    }

    levelSerializer.Skybox.Read(m_LevelSettings.Skybox);
    levelSerializer.AmbientColor.Read(m_LevelSettings.AmbientColor);
    levelSerializer.FogColor.Read(m_LevelSettings.FogColor);
    levelSerializer.FogIntensity.Read(m_LevelSettings.FogIntensity);
    levelSerializer.FogDistance.Read(m_LevelSettings.FogDistance);
    levelSerializer.Camera.Read(m_LevelSettings.CameraEntity);

    m_State = AssetState::Loaded;

    return true;
}

bool Pine::Level::SaveToFile()
{
    LevelSerializer levelSerializer;

    for (auto bp : m_Blueprints)
    {
        levelSerializer.Blueprints.AddData(bp->ToByteSpan());
    }

    levelSerializer.Camera.Write(m_LevelSettings.HasCamera ? m_LevelSettings.CameraEntity : 0);

    levelSerializer.Skybox.Write(m_LevelSettings.Skybox);
    levelSerializer.AmbientColor.Write(m_LevelSettings.AmbientColor);
    levelSerializer.FogColor.Write(m_LevelSettings.FogColor);
    levelSerializer.FogIntensity.Write(m_LevelSettings.FogIntensity);
    levelSerializer.FogDistance.Write(m_LevelSettings.FogDistance);

    File::WriteCompressed(m_FilePath, levelSerializer.Write());

    return true;
}

void Pine::Level::Dispose()
{
    ClearBlueprints();
}
#include "Level.hpp"
#include "../../Core/Serialization/Json/SerializationJson.hpp"
#include "Pine/Core/File/File.hpp"
#include "Pine/World/World.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"

namespace
{
    void FindCameraOrdinal(const Pine::Entity* entity, const Pine::Entity* cameraEntity,
        std::uint32_t& ordinal, std::uint32_t& cameraOrdinal)
    {
        ++ordinal;
        if (entity == cameraEntity)
        {
            cameraOrdinal = ordinal;
        }
        for (const auto child : entity->GetChildren())
        {
            FindCameraOrdinal(child, cameraEntity, ordinal, cameraOrdinal);
        }
    }
}

bool Pine::Level::LoadAssetData(const ByteSpan& span)
{
    LevelSerializer levelSerializer;

    if (!levelSerializer.Read(span))
    {
        return false;
    }

    ClearBlueprints();
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
    levelSerializer.Exposure.Read(m_LevelSettings.Exposure);
    levelSerializer.BloomThreshold.Read(m_LevelSettings.BloomThreshold);
    levelSerializer.BloomIntensity.Read(m_LevelSettings.BloomIntensity);
    levelSerializer.GrainStrength.Read(m_LevelSettings.GrainStrength);
    levelSerializer.VignetteStrength.Read(m_LevelSettings.VignetteStrength);
    levelSerializer.WindDirection.Read(m_LevelSettings.WindDirection);
    levelSerializer.WindStrength.Read(m_LevelSettings.WindStrength);
    levelSerializer.WindSpeed.Read(m_LevelSettings.WindSpeed);
    m_LevelSettings.CameraEntity = 0;
    levelSerializer.Camera.Read(m_LevelSettings.CameraEntity);
    m_CameraUsesSerializedOrder = false;
    levelSerializer.CameraUsesSerializedOrder.Read(m_CameraUsesSerializedOrder);
    m_LevelSettings.HasCamera = m_LevelSettings.CameraEntity != 0;

    return true;
}

Pine::ByteSpan Pine::Level::SaveAssetData()
{
    LevelSerializer levelSerializer;

    for (auto bp : m_Blueprints)
    {
        levelSerializer.Blueprints.AddData(bp->ToByteSpan());
    }

    const auto cameraIndex = m_LevelSettings.HasCamera ? m_LevelSettings.CameraEntity : 0;
    levelSerializer.Camera.Write(cameraIndex);
    levelSerializer.CameraUsesSerializedOrder.Write(m_CameraUsesSerializedOrder);

    levelSerializer.Skybox.Write(m_LevelSettings.Skybox);
    levelSerializer.AmbientColor.Write(m_LevelSettings.AmbientColor);
    levelSerializer.FogColor.Write(m_LevelSettings.FogColor);
    levelSerializer.FogIntensity.Write(m_LevelSettings.FogIntensity);
    levelSerializer.FogDistance.Write(m_LevelSettings.FogDistance);
    levelSerializer.Exposure.Write(m_LevelSettings.Exposure);
    levelSerializer.BloomThreshold.Write(m_LevelSettings.BloomThreshold);
    levelSerializer.BloomIntensity.Write(m_LevelSettings.BloomIntensity);
    levelSerializer.GrainStrength.Write(m_LevelSettings.GrainStrength);
    levelSerializer.VignetteStrength.Write(m_LevelSettings.VignetteStrength);
    levelSerializer.WindDirection.Write(m_LevelSettings.WindDirection);
    levelSerializer.WindStrength.Write(m_LevelSettings.WindStrength);
    levelSerializer.WindSpeed.Write(m_LevelSettings.WindSpeed);

    return levelSerializer.Write();
}

Pine::Level::Level()
{
    m_Type = AssetType::Level;
}

void Pine::Level::CreateFromWorld()
{
    auto primaryRenderingContext = RenderManager::GetPrimaryRenderingContext();
    const auto currentCameraEntity = primaryRenderingContext->SceneCamera != nullptr ? primaryRenderingContext->SceneCamera->GetParent() : nullptr;

    ClearBlueprints();

    // Blueprint::Spawn recreates roots and descendants in depth-first order. Live
    // scene order can differ after parenting, and includes editor-only entities.
    m_CameraUsesSerializedOrder = true;
    m_LevelSettings.CameraEntity = 0;
    std::uint32_t ordinal = 0;

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
        FindCameraOrdinal(entity, currentCameraEntity, ordinal, m_LevelSettings.CameraEntity);
    }

    m_LevelSettings.HasCamera = m_LevelSettings.CameraEntity != 0;
    m_LevelSettings.Skybox = primaryRenderingContext->Skybox;
}

void Pine::Level::Load()
{
    auto primaryRenderingContext = RenderManager::GetPrimaryRenderingContext();

    primaryRenderingContext->SceneCamera = nullptr;
    Entities::DeleteAll();

    const auto entityOffset = Entities::GetList().size();

    for (const auto& blueprint : m_Blueprints)
    {
        blueprint->Spawn();
    }

    if (m_LevelSettings.HasCamera)
    {
        const auto& entityList = Entities::GetList();

        // New assets store a one-based ordinal within the serialized hierarchy. Older ones stored a
        // one-based index into the editor's live entity list, which counted the editor's own
        // (temporary) entity ahead of the scene - hence the extra step back. Both are then placed
        // after however many entities survived the scene reset. Re-saving migrates the asset.
        const auto serializedOrdinal = static_cast<std::int64_t>(m_LevelSettings.CameraEntity)
            - (m_CameraUsesSerializedOrder ? 0 : 1);

        const auto entityCameraIndex = static_cast<std::int64_t>(entityOffset) + serializedOrdinal - 1;

        if (entityCameraIndex >= 0 && entityCameraIndex < static_cast<std::int64_t>(entityList.size()))
        {
            primaryRenderingContext->SceneCamera = entityList[entityCameraIndex]->GetComponent<Camera>();
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
}

std::size_t Pine::Level::GetBlueprintCount() const
{
    return m_Blueprints.size();
}

Pine::LevelSettings& Pine::Level::GetLevelSettings()
{
    return m_LevelSettings;
}
/*

bool Pine::Level::LoadFromFile(AssetLoadStage stage)
{
    const auto fileByteSpan = File::ReadCompressed(m_FilePath);

    if (!fileByteSpan.data)
    {
        return false;
    }


    m_State = AssetState::Loaded;

    return true;
}

bool Pine::Level::SaveToFile()
{


    File::WriteCompressed(m_FilePath, levelSerializer.Write());

    return true;
}
*/

void Pine::Level::Dispose()
{
    ClearBlueprints();
}

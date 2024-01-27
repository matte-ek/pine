#include "Level.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"
#include "Pine/World/World.hpp"
#include "Pine/World/Entities/Entities.hpp"

Pine::Level::Level()
{
    m_Type = AssetType::Level;
    m_LoadMode = AssetLoadMode::MultiThread;
}

void Pine::Level::CreateFromWorld()
{
    ClearBlueprints();

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
}

void Pine::Level::Load()
{
    Entities::DeleteAll();

    for (auto blueprint : m_Blueprints)
    {
        blueprint->Spawn();
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

bool Pine::Level::LoadFromFile(AssetLoadStage stage)
{
    const auto json = Serialization::LoadFromFile(m_FilePath);

    if (!json.has_value())
    {
        return false;
    }

    if (json.value().contains("entities"))
    {
        for (const auto& blueprintJson : json.value()["entities"])
        {
            auto blueprint = new Blueprint();

            blueprint->FromJson(blueprintJson);

            m_Blueprints.push_back(blueprint);
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

    Serialization::SaveToFile(m_FilePath, j);

    return true;
}

void Pine::Level::Dispose()
{
    ClearBlueprints();
}
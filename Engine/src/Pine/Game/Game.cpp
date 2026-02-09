#include "Game.hpp"

#include <filesystem>

#include "Pine/Assets/Level/Level.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "../Core/Serialization/Json/SerializationJson.hpp"
#include "Pine/Engine/Engine.hpp"
#include "Pine/World/World.hpp"

namespace
{
    Pine::Game::GameProperties m_GameProperties;
}

void Pine::Game::SetGameProperties(const GameProperties& gameProperties)
{
    m_GameProperties = gameProperties;
}

const Pine::Game::GameProperties& Pine::Game::GetGameProperties()
{
    return m_GameProperties;
}

void Pine::Game::Setup()
{
    auto json = SerializationJson::LoadFromFile("game/game.json");

    if (!json.has_value())
    {
        if (Engine::GetEngineConfiguration().m_ProductionMode)
        {
            Log::Warning("Game file missing or corrupted, game might not start properly.");
        }

        return;
    }

    const auto& j = json.value();

    SerializationJson::LoadValue(j, "name", m_GameProperties.Name);
    SerializationJson::LoadValue(j, "version", m_GameProperties.Version);
    SerializationJson::LoadValue(j, "author", m_GameProperties.Author);

    for (int i = 0; i < 64;i++)
    {
        SerializationJson::LoadValue(j, fmt::format("tag{}", i), m_GameProperties.EntityTags[i]);
    }

    for (int i = 0; i < 31;i++)
    {
        SerializationJson::LoadValue(j, fmt::format("layer{}", i), m_GameProperties.ColliderLayers[i]);
    }

    SerializationJson::LoadValue(j, "startupLevel", m_GameProperties.StartupLevel);
}

void Pine::Game::OnStart()
{
    if (m_GameProperties.StartupLevel.empty())
    {
        return;
    }

    auto asset = Pine::Assets::Get<Level>(m_GameProperties.StartupLevel);
    if (!asset)
    {
        Log::Warning(fmt::format("Referenced startup level {} could not be found.", m_GameProperties.StartupLevel));
    }

    World::SetActiveLevel(asset);
}

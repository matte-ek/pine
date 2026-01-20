#include "Game.hpp"

#include <filesystem>

#include "Pine/Assets/Level/Level.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"
#include "Pine/Engine/Engine.hpp"
#include "Pine/World/World.hpp"

namespace
{
    Pine::Game::GameProperties m_GameProperties;
}

const Pine::Game::GameProperties& Pine::Game::GetGameProperties()
{
    return m_GameProperties;
}

void Pine::Game::Setup()
{
    auto json = Serialization::LoadFromFile("game.json");

    if (!json.has_value())
    {
        if (Engine::GetEngineConfiguration().m_ProductionMode)
        {
            Log::Warning("Game file missing or corrupted, game might not start properly.");
        }

        return;
    }

    const auto& j = json.value();

    Serialization::LoadValue(j, "name", m_GameProperties.Name);
    Serialization::LoadValue(j, "version", m_GameProperties.Version);
    Serialization::LoadValue(j, "author", m_GameProperties.Author);
    Serialization::LoadValue(j, "startupLevel", m_GameProperties.StartupLevel);
}

void Pine::Game::OnStartup()
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

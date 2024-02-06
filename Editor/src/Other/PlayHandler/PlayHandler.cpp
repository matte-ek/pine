#include <cassert>
#include "PlayHandler.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/World/World.hpp"
#include "Pine/Script/Runtime/ScriptingRuntime.hpp"

namespace
{
    PlayHandler::EditorGameState m_GameState = PlayHandler::EditorGameState::Stopped;

    Pine::Level m_LevelSnapshot;
}

void PlayHandler::Play()
{
    assert(m_GameState == EditorGameState::Stopped);

    m_GameState = EditorGameState::Playing;
    m_LevelSnapshot.CreateFromWorld();

    Pine::World::SetPaused(false);
    Pine::World::OnStart();
}

void PlayHandler::Pause()
{
    assert(m_GameState == EditorGameState::Playing);

    m_GameState = EditorGameState::Paused;

    Pine::World::SetPaused(true);
}

void PlayHandler::Stop()
{
    assert(m_GameState == EditorGameState::Playing || m_GameState == EditorGameState::Paused);

    m_GameState = EditorGameState::Stopped;

    auto oldLoadedLevel = Pine::World::GetActiveLevel();

    m_LevelSnapshot.Load();

    Pine::World::SetActiveLevel(oldLoadedLevel, true);
    Pine::World::SetPaused(true);

    Pine::Script::Runtime::RunGarbageCollector();
}

PlayHandler::EditorGameState PlayHandler::GetGameState()
{
    return m_GameState;
}

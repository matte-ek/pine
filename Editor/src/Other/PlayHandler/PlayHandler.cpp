#include <cassert>
#include "PlayHandler.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/World/World.hpp"
#include "Pine/Script/Runtime/ScriptingRuntime.hpp"
#include "Gui/Shared/Selection/Selection.hpp"

namespace
{
    PlayHandler::EditorGameState m_GameState = PlayHandler::EditorGameState::Stopped;

    Pine::Level m_LevelSnapshot;

    // The snapshot above only captures entities; the level's own settings, which scripts can change
    // through Level.Rendering, are kept here along with the level they belong to.
    Pine::Level* m_PlayedLevel = nullptr;
    Pine::LevelSettings m_PlayedLevelSettings;
}

void PlayHandler::Play()
{
    assert(m_GameState == EditorGameState::Stopped);

    m_GameState = EditorGameState::Playing;
    m_LevelSnapshot.CreateFromWorld();

    m_PlayedLevel = Pine::World::GetActiveLevel();
    if (m_PlayedLevel != nullptr)
    {
        m_PlayedLevelSettings = m_PlayedLevel->GetLevelSettings();
    }

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

    if (oldLoadedLevel != nullptr && oldLoadedLevel == m_PlayedLevel)
    {
        oldLoadedLevel->GetLevelSettings() = m_PlayedLevelSettings;
    }

    m_PlayedLevel = nullptr;

    Selection::Clear();

    Pine::Script::Runtime::RunGarbageCollector();
}

PlayHandler::EditorGameState PlayHandler::GetGameState()
{
    return m_GameState;
}

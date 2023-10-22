#include <GLFW/glfw3.h>
#include "World.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/Physics/Physics3D/Physics3D.hpp"

namespace
{

    Pine::Level *m_Level = nullptr;

    float m_TimeScale = 1.f;
    bool m_Paused = false;

    double m_CurrentTime = 0.0;

    double m_LastRenderTime = 0.0;
    double m_LastUpdateTime = 0.0;

    double CalculateFrameTime()
    {
        // Calculate delta time
        m_CurrentTime = glfwGetTime();

        if (m_LastRenderTime == 0.0)
        {
            m_LastRenderTime = m_CurrentTime;
        }

        const double deltaTime = m_CurrentTime - m_LastRenderTime;

        m_LastRenderTime = m_CurrentTime;

        return deltaTime;
    }
}

void Pine::World::SetPaused(bool value)
{
    m_Paused = value;
}

bool Pine::World::IsPaused()
{
    return m_Paused;
}

void Pine::World::SetActiveLevel(Level *level, bool ignoreLoad)
{
    if (!ignoreLoad)
    {
        level->Load();

        // We can return here since Level::Load will call this method again with ignoreLoad,
        // so m_Level will always be set.
        return;
    }

    m_Level = level;
}

Pine::Level *Pine::World::GetActiveLevel()
{
    return m_Level;
}

void Pine::World::SetTimeScale(float value)
{
    m_TimeScale = value;
}

float Pine::World::GetTimeScale()
{
    return m_TimeScale;
}

void Pine::World::Update()
{
    const auto deltaTime = CalculateFrameTime();

    Physics3D::Update(deltaTime);
}

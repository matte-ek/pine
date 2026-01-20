#include "ScopedTimer.hpp"

#include "Pine/Performance/Performance.hpp"

Pine::ScopedTimer::ScopedTimer(Performance::TrackedScope* scope)
{
    m_TrackedScope = scope;
}

Pine::ScopedTimer::~ScopedTimer()
{
    Stop();
}

void Pine::ScopedTimer::Stop()
{
    m_Timer.Stop();

    m_TrackedScope->Time = m_Timer.GetElapsedTime();
}

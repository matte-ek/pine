#include "ScopedTimer.hpp"

#include "Pine/Performance/Performance.hpp"

Pine::ScopedTimer::ScopedTimer(Performance::TrackedScope* scope)
{
    m_TrackedScope = scope;

    Performance::Internal::EnterScope(scope);
}

Pine::ScopedTimer::~ScopedTimer()
{
    Stop();
}

void Pine::ScopedTimer::Stop()
{
    if (m_HasStopped)
    {
        return;
    }

    m_HasStopped = true;

    m_Timer.Stop();

    Performance::Internal::ExitScope(m_TrackedScope, m_Timer.GetElapsedTime());
}

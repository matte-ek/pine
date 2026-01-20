#pragma once
#include "Pine/Core/Timer/Timer.hpp"

namespace Pine
{
    namespace Performance
    {
        struct TrackedScope;
    }

    class ScopedTimer
    {
    private:
        Performance::TrackedScope* m_TrackedScope;

        Timer m_Timer;
    public:
        explicit ScopedTimer(Performance::TrackedScope* scope);
        ~ScopedTimer();

        void Stop();
    };
}

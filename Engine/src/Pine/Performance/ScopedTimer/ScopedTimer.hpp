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

        bool m_HasStopped = false;
    public:
        explicit ScopedTimer(Performance::TrackedScope* scope);
        ~ScopedTimer();

        // Ends the measurement early. The destructor will not measure again, since the scope sums
        // its invocations and counting this one twice would inflate both numbers.
        void Stop();
    };
}

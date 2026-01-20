#pragma once
#include <chrono>

namespace Pine
{

    class Timer
    {
    private:
        std::chrono::high_resolution_clock::time_point m_StartTime;
        std::chrono::high_resolution_clock::time_point m_EndTime;
    public:
        Timer();

        void Stop();

        double GetElapsedTime() const;
    };

}
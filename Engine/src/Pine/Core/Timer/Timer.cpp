#include "Timer.hpp"

Pine::Timer::Timer()
{
    m_StartTime = std::chrono::high_resolution_clock::now();
}

void Pine::Timer::Stop()
{
    m_EndTime = std::chrono::high_resolution_clock::now();
}

double Pine::Timer::GetElapsedTime() const
{
    return std::chrono::duration<double>(m_EndTime - m_StartTime).count();
}

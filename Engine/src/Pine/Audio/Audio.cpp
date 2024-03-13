#include "Audio.hpp"

namespace
{
    Pine::Audio::IAudioAPI* m_IAudioAPI = nullptr;
}

bool Pine::Audio::Setup()
{
    m_IAudioAPI = new OpenAL();

    return m_IAudioAPI->Setup();
}

void Pine::Audio::Shutdown()
{
    m_IAudioAPI->Shutdown();
}
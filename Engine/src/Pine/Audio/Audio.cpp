#include "Audio.hpp"

#include "OpenAL/OpenAL.hpp"

namespace
{
    Pine::Audio::IAudioAPI* m_AudioAPI = nullptr;
}

bool Pine::Audio::Setup()
{
    const auto audioAPI = new OpenAL();

    if (!audioAPI->Setup())
    {
        delete audioAPI;

        return false;
    }

    m_AudioAPI = audioAPI;

    return true;
}

void Pine::Audio::Shutdown()
{
    if (m_AudioAPI == nullptr)
    {
        return;
    }

    m_AudioAPI->Shutdown();

    delete m_AudioAPI;

    m_AudioAPI = nullptr;
}

Pine::Audio::IAudioAPI* Pine::Audio::GetAudioAPI()
{
    return m_AudioAPI;
}

bool Pine::Audio::HasInitializedAudioAPI()
{
    return m_AudioAPI != nullptr;
}

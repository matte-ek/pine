#include "AudioListener.hpp"

#include <algorithm>

Pine::AudioListener::AudioListener()
    : Component(ComponentType::AudioListener)
{
}

void Pine::AudioListener::SetVolume(const float volume)
{
    m_Volume = std::max(volume, 0.f);
}

float Pine::AudioListener::GetVolume() const
{
    return m_Volume;
}

void Pine::AudioListener::LoadData(const ByteSpan& span)
{
    AudioListenerSerializer serializer;

    serializer.Read(span);

    serializer.Volume.Read(m_Volume);
}

Pine::ByteSpan Pine::AudioListener::SaveData()
{
    AudioListenerSerializer serializer;

    serializer.Volume.Write(m_Volume);

    return serializer.Write();
}

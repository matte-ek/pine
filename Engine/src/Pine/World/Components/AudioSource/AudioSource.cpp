#include "AudioSource.hpp"
#include "Pine/Assets/AudioFile/AudioFile.hpp"

Pine::AudioSource::AudioSource()
    : IComponent(ComponentType::AudioSource)
{
}

void Pine::AudioSource::SetAudioFile(AudioFile *file)
{
    m_AudioFile = file;
}

Pine::AudioFile * Pine::AudioSource::GetAudioFile() const
{
    return m_AudioFile.Get();
}

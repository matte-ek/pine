#include "AudioSource.hpp"
#include "Pine/Assets/AudioFile/AudioFile.hpp"

Pine::AudioSource::AudioSource()
    : Component(ComponentType::AudioSource)
{
}

void Pine::AudioSource::Play() const
{
    alSourcePlay(m_SourceId);
}

bool Pine::AudioSource::IsPlaying() const
{
    if (m_SourceId == 0)
        return false;

    ALint state;
    alGetSourcei(m_SourceId, AL_SOURCE_STATE, &state);
    return state == AL_PLAYING;
}

float Pine::AudioSource::GetPlaybackPosition() const
{
    if (m_SourceId == 0)
        return 0.0f;

    ALfloat seconds;
    alGetSourcef(m_SourceId, AL_SEC_OFFSET, &seconds);
    return seconds;
}

void Pine::AudioSource::SetPlayOnStart(bool playOnStart)
{
    m_PlayOnStart = playOnStart;
}

bool Pine::AudioSource::GetPlayOnStart() const
{
    return m_PlayOnStart;
}

void Pine::AudioSource::SetVolume(float volume) const
{
    if (m_SourceId == 0)
        return;

    alSourcef(m_SourceId, AL_GAIN, volume);
}

float Pine::AudioSource::GetVolume() const
{
    if (m_SourceId == 0)
        return 0.0f;

    ALfloat volume;
    alGetSourcef(m_SourceId, AL_GAIN, &volume);
    return volume;
}

void Pine::AudioSource::OnSetup()
{
    Component::OnSetup();

    if (m_Standalone)
        return;

    if (m_AudioFile == nullptr)
        return;

    if (m_PlayOnStart)
        alSourcePlay(m_SourceId);
}

void Pine::AudioSource::SetAudioFile(AudioFile *file)
{
    if (file != nullptr)
        return;

    m_AudioFile = file;
    m_SourceId = m_AudioFile->GetNewSource();
}

Pine::AudioFile * Pine::AudioSource::GetAudioFile() const
{
    return m_AudioFile.Get();
}

#include "AudioSource.hpp"
#include "Pine/Assets/AudioFile/AudioFile.hpp"
#include "Pine/Audio/Audio.hpp"

Pine::AudioSource::AudioSource()
    : Component(ComponentType::AudioSource)
{
}

void Pine::AudioSource::Play() const
{
    if (m_SourceId == 0)
    {
        return;
    }

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

void Pine::AudioSource::SetPlayOnStart(const bool playOnStart)
{
    m_PlayOnStart = playOnStart;
}

bool Pine::AudioSource::GetPlayOnStart() const
{
    return m_PlayOnStart;
}

void Pine::AudioSource::SetVolume(const float volume) const
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
        Play();
}

void Pine::AudioSource::SetAudioFile(AudioFile *file)
{
    m_AudioFile = file;

    // Without an output device there is nothing to generate a source on, and every call below
    // would only be setting OpenAL's error flag.
    if (m_SourceId == 0 && Audio::HasInitializedAudioAPI())
    {
        alGenSources(1, &m_SourceId);
    }

    if (m_SourceId == 0)
    {
        return;
    }

    const auto buffer = file != nullptr ? file->GetBuffer() : nullptr;

    // Sources are still driven through OpenAL from here rather than through IAudioAPI, unlike the
    // buffer they play. That is the next piece of the audio work, along with everything else this
    // component does not do yet - serializing its file, following its entity, being stopped.
    const auto bufferId = buffer != nullptr
        ? static_cast<ALint>(*static_cast<std::uint32_t*>(buffer->GetAudioIdentifier()))
        : 0;

    alSourcei(m_SourceId, AL_BUFFER, bufferId);
}

Pine::AudioFile * Pine::AudioSource::GetAudioFile() const
{
    return m_AudioFile.Get();
}

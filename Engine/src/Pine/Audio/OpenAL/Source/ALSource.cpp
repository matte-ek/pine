#include "ALSource.hpp"

#include <AL/al.h>

#include "Pine/Audio/OpenAL/OpenAL.hpp"

// The parameter setters here deliberately do not check OpenAL's error flag, unlike the rest of the
// backend. Pine::Audio pushes every one of them onto every playing voice once a frame, so checking
// per call would be a few hundred alGetError() calls a frame reporting on values AudioSource has
// already clamped into range. The calls that can genuinely fail - generating the source, binding a
// clip, starting playback - still check.

bool Pine::Audio::ALSource::Setup()
{
    ClearAudioError();

    alGenSources(1, &m_Id);

    if (CheckAudioError("Generating an audio source"))
    {
        m_Id = 0;

        return false;
    }

    return true;
}

std::uint32_t Pine::Audio::ALSource::GetId() const
{
    return m_Id;
}

void Pine::Audio::ALSource::SetBuffer(IAudioBuffer* buffer)
{
    ClearAudioError();

    // A source has to be stopped before its buffer can be swapped - OpenAL refuses the change
    // otherwise, and playing on into a different clip from the old clip's position would be
    // meaningless anyway.
    alSourceStop(m_Id);

    const auto bufferId = buffer != nullptr
        ? static_cast<ALint>(*static_cast<std::uint32_t*>(buffer->GetAudioIdentifier()))
        : 0;

    alSourcei(m_Id, AL_BUFFER, bufferId);

    CheckAudioError("Binding a clip to an audio source");
}

void Pine::Audio::ALSource::Play()
{
    ClearAudioError();

    alSourcePlay(m_Id);

    CheckAudioError("Starting audio playback");
}

void Pine::Audio::ALSource::Pause()
{
    alSourcePause(m_Id);
}

void Pine::Audio::ALSource::Stop()
{
    alSourceStop(m_Id);
}

Pine::Audio::PlaybackState Pine::Audio::ALSource::GetState() const
{
    ALint state = AL_STOPPED;

    alGetSourcei(m_Id, AL_SOURCE_STATE, &state);

    switch (state)
    {
        case AL_PLAYING:
            return PlaybackState::Playing;
        case AL_PAUSED:
            return PlaybackState::Paused;
        default:
            // AL_INITIAL - a source that has never been played - counts as stopped: nothing is
            // coming out of it, which is the only distinction the caller acts on.
            return PlaybackState::Stopped;
    }
}

void Pine::Audio::ALSource::SetSpatial(const bool spatial)
{
    // AL_SOURCE_RELATIVE measures a source's position from the listener instead of from the world
    // origin, so a non-spatial source is simply one sitting at an offset of zero from the ears:
    // no panning, no attenuation, identical wherever the listener walks. That is the whole of 2D
    // audio in OpenAL.
    alSourcei(m_Id, AL_SOURCE_RELATIVE, spatial ? AL_FALSE : AL_TRUE);

    if (!spatial)
    {
        alSource3f(m_Id, AL_POSITION, 0.f, 0.f, 0.f);
    }
}

void Pine::Audio::ALSource::SetPosition(const Vector3f& position)
{
    alSource3f(m_Id, AL_POSITION, position.x, position.y, position.z);
}

void Pine::Audio::ALSource::SetVolume(const float volume)
{
    alSourcef(m_Id, AL_GAIN, volume);
}

void Pine::Audio::ALSource::SetPitch(const float pitch)
{
    alSourcef(m_Id, AL_PITCH, pitch);
}

void Pine::Audio::ALSource::SetLooping(const bool loop)
{
    alSourcei(m_Id, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
}

void Pine::Audio::ALSource::SetAttenuation(
    const float referenceDistance,
    const float maxDistance,
    const float rolloffFactor)
{
    alSourcef(m_Id, AL_REFERENCE_DISTANCE, referenceDistance);
    alSourcef(m_Id, AL_MAX_DISTANCE, maxDistance);
    alSourcef(m_Id, AL_ROLLOFF_FACTOR, rolloffFactor);
}

void Pine::Audio::ALSource::SetPlaybackPosition(const float seconds)
{
    alSourcef(m_Id, AL_SEC_OFFSET, seconds);
}

float Pine::Audio::ALSource::GetPlaybackPosition() const
{
    ALfloat seconds = 0.f;

    alGetSourcef(m_Id, AL_SEC_OFFSET, &seconds);

    return seconds;
}

void Pine::Audio::ALSource::Dispose()
{
    if (m_Id == 0)
    {
        return;
    }

    alSourceStop(m_Id);
    alDeleteSources(1, &m_Id);

    m_Id = 0;
}

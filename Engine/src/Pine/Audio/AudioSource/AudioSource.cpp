#include "AudioSource.hpp"

namespace Pine::Audio
{

    AudioSource::AudioSource(ALuint buffer)
    {
        // TODO
        alGenSources(1, &m_id);
        alSourcei(m_id, AL_BUFFER, buffer);
    }

    AudioSource::~AudioSource()
    {
        // TODO
        alDeleteSources(1, &m_id);
    }

    ALuint AudioSource::GetID() const
    {
        return m_id;
    }

    float AudioSource::GetSeconds() const {
        ALfloat pos = 0.0f;
        alGetSourcef(m_id, AL_SEC_OFFSET, &pos);
        return pos;
    }


}
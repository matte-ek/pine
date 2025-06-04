#include "AudioSourceObject.hpp"

namespace Pine::Audio
{

    AudioSourceObject::AudioSourceObject(ALuint buffer)
    {
        // TODO
        alGenSources(1, &m_id);
        alSourcei(m_id, AL_BUFFER, buffer);
    }

    AudioSourceObject::~AudioSourceObject()
    {
        // TODO
        alDeleteSources(1, &m_id);
    }

    ALuint AudioSourceObject::GetID() const
    {
        return m_id;
    }

    float AudioSourceObject::GetSeconds() const
    {
        ALfloat pos = 0.0f;
        alGetSourcef(m_id, AL_SEC_OFFSET, &pos);
        return pos;
    }


}
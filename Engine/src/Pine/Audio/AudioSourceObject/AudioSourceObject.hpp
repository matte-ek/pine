#pragma once

#include <AL/al.h>

namespace Pine::Audio
{
    class AudioSourceObject
    {
    private:
        ALuint m_id;

    public:
        AudioSourceObject(ALuint buffer);
        ~AudioSourceObject();

        ALuint GetID() const;
        float GetSeconds() const;
    };
}

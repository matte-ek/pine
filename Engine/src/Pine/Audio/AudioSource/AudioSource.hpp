#pragma once

#include <AL/al.h>

namespace Pine::Audio
{
    class AudioSource
    {
    private:
        ALuint m_id;

    public:
        AudioSource(ALuint buffer);
        ~AudioSource();

        ALuint GetID() const;
    };
}


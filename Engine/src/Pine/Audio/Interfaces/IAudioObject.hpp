#pragma once

namespace Pine::Audio
{
    class IAudioObject
    {
    public:
        virtual ~IAudioObject() = default;

        virtual bool Setup() = 0;
        virtual void Dispose() = 0;

        virtual void Play() = 0;
        virtual void Stop() = 0;
        virtual int GetID() = 0;
        virtual float GetDuration() = 0;
        virtual bool Transcode() = 0;
    };
}
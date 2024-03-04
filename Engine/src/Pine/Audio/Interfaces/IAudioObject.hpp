#pragma once

namespace Pine::Audio
{
    class IAudioObject
    {
    public:
        virtual ~IAudioObject() = default;
        virtual bool Setup() = 0;
        virtual bool Transcode() = 0;
        virtual void Dispose() = 0;
    };
}
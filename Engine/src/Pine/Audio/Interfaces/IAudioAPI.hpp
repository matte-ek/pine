#pragma once

#include <string>
#include <vector>

namespace Pine::Audio
{
    class IAudioAPI
    {
    public:
        IAudioAPI() = default;

        virtual ~IAudioAPI() = default;

        virtual bool Setup() = 0;

        virtual void Shutdown() = 0;
    };
}
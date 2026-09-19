#pragma once

#include "IAudioBuffer.hpp"

namespace Pine::Audio
{
    class IAudioAPI
    {
    public:
        IAudioAPI() = default;

        virtual ~IAudioAPI() = default;

        virtual bool Setup() = 0;

        virtual void Shutdown() = 0;

        virtual IAudioBuffer* CreateBuffer() = 0;
        virtual void DestroyBuffer(IAudioBuffer* buffer) = 0;
    };
}

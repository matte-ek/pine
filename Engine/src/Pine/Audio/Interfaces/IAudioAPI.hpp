#pragma once

#include "IAudioBuffer.hpp"
#include "IAudioSource.hpp"

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

        // Returns nullptr once the device will not give out any more voices, which is an ordinary
        // answer rather than a failure - every implementation caps them, and well below the number
        // of sounds a level holds. Pine::Audio reserves its whole budget up front on that basis.
        virtual IAudioSource* CreateSource() = 0;
        virtual void DestroySource(IAudioSource* source) = 0;

        // The listener is device state rather than an object: there is exactly one, and every
        // spatial source is heard from it. 'forward' and 'up' are expected to be unit length and
        // perpendicular to each other.
        virtual void SetListenerTransform(const Vector3f& position, const Vector3f& forward, const Vector3f& up) = 0;

        // Master volume, applied on top of whatever each individual source is set to.
        virtual void SetListenerVolume(float volume) = 0;
    };
}

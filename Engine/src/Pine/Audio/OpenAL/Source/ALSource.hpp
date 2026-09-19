#pragma once

#include <cstdint>

#include "Pine/Audio/Interfaces/IAudioSource.hpp"

namespace Pine::Audio
{
    class ALSource final : public IAudioSource
    {
    private:
        std::uint32_t m_Id = 0;
    public:
        // Generates the underlying OpenAL source, and returns false when the device will not give
        // out another one. Kept out of the constructor because running out of sources is expected
        // rather than exceptional, and the pool needs a plain answer to build on.
        bool Setup();

        std::uint32_t GetId() const;

        void SetBuffer(IAudioBuffer* buffer) override;

        void Play() override;
        void Pause() override;
        void Stop() override;

        PlaybackState GetState() const override;

        void SetSpatial(bool spatial) override;
        void SetPosition(const Vector3f& position) override;

        void SetVolume(float volume) override;
        void SetPitch(float pitch) override;
        void SetLooping(bool loop) override;

        void SetAttenuation(float referenceDistance, float maxDistance, float rolloffFactor) override;

        void SetPlaybackPosition(float seconds) override;
        float GetPlaybackPosition() const override;

        void Dispose() override;
    };
}

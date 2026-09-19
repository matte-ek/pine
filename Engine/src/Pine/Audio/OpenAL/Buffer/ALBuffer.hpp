#pragma once

#include <cstdint>

#include "Pine/Audio/Interfaces/IAudioBuffer.hpp"

namespace Pine::Audio
{
    class ALBuffer final : public IAudioBuffer
    {
    private:
        std::uint32_t m_Id = 0;
    public:
        void* GetAudioIdentifier() override;
        std::uint32_t GetId() const;

        bool Upload(const void* data, AudioFormat format, int sampleRate, int sampleCount) override;
        void Dispose() override;
    };
}

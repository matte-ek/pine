#pragma once

#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/Audio/WaveFile/WaveFile.hpp"
#include "Pine/Audio/AudioSourceObject/AudioSourceObject.hpp"

namespace Pine
{
    enum class AudioFileFormat
    {
        Wave,
        Flac,
        Ogg,
        Unknown
    };

    class AudioFile : public IAsset
    {
    private:
        AudioFileFormat m_AudioFileFormat = AudioFileFormat::Unknown;

        Audio::IAudioObject* m_AudioObject = nullptr;

        bool ProcessFile();
    public:
        AudioFile();

        float GetDuration() const;
        ALuint GetNewSource() const;

        bool LoadFromFile(AssetLoadStage stage) override;
        void Dispose() override;
    };
}




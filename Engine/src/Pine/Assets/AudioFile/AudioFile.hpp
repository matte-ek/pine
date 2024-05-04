#pragma once

#include <cstring>
#include <cstdio>

#include "Pine/Core/File/File.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/Audio/WaveFile/WaveFile.hpp"
#include "Pine/Audio/AudioSource/AudioSource.hpp"

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
        std::string m_FileExtension;
        AudioFileFormat m_AudioFileFormat = AudioFileFormat::Unknown;
        Pine::Audio::IAudioObject* m_AudioObject = nullptr;
        Pine::Audio::AudioSource* m_AudioSource = nullptr;

        AudioFileFormat GetAudioFileFormat();
    public:
        AudioFile();

        bool Setup();
        void Play();
        bool Transcode();
        bool LoadFromFile(AssetLoadStage stage) override;
        void Dispose() override;
    };
}




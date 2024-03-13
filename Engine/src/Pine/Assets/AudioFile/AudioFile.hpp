#pragma once

#include <cstring>
#include <cstdio>
#include <fstream>
#include "Pine/Core/File/File.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/Audio/WaveFile/WaveFile.hpp"

enum class AudioFileFormat
{
    Wave,
    Flac,
    Ogg,
    Unknown
};

namespace Pine
{
    class AudioFile : public IAsset
    {
    private:
        std::string m_FileExtension;
        AudioFileFormat m_AudioFileFormat = AudioFileFormat::Unknown;
        Pine::Audio::IAudioObject* m_AudioObject = nullptr;

        AudioFileFormat GetAudioFileFormat();

    public:
        AudioFile();

        bool Setup();
        bool Transcode();
        bool LoadFromFile(AssetLoadStage stage) override;
        void Dispose() override;
    };
}




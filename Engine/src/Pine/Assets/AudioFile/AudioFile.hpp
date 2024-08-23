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

    enum class AudioState
    {
        Playing,
        Paused,
        Stopped
    };

    class AudioFile : public IAsset
    {
    private:
        std::string m_FileExtension;
        AudioFileFormat m_AudioFileFormat = AudioFileFormat::Unknown;
        Audio::IAudioObject* m_AudioObject = nullptr;
        Audio::AudioSource* m_AudioSource = nullptr;
        AudioState m_AudioState = AudioState::Stopped;

        AudioFileFormat GetAudioFileFormat() const;
    public:
        AudioFile();

        bool Setup();
        void Play();
        void Stop();
        void Pause();
        bool Transcode();
        AudioState GetState() const;
        float GetTime() const;
        float GetDuration() const;
        bool LoadFromFile(AssetLoadStage stage) override;
        void Dispose() override;
    };
}




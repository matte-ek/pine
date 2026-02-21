#pragma once

#include "Pine/Assets/Asset/Asset.hpp"
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

    enum class AudioState
    {
        Playing,
        Paused,
        Stopped
    };

    class AudioFile : public Asset
    {
    private:
        AudioFileFormat m_AudioFileFormat = AudioFileFormat::Unknown;

        Audio::IAudioObject* m_AudioObject = nullptr;
        Audio::AudioSourceObject* m_AudioSource = nullptr;

        AudioState m_AudioState = AudioState::Stopped;

        bool ProcessFile();
    public:
        AudioFile();

        void Play();
        void Stop();
        void Pause();

        bool Transcode();

        AudioState GetAudioState() const;

        float GetTime() const;
        float GetDuration() const;
        ALuint GetNewSource() const;

        void Dispose() override;
    };
}




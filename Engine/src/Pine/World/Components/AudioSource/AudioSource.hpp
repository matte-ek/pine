#pragma once
#include "Pine/World/Components/IComponent/IComponent.hpp"
#include "Pine/Assets/IAsset/IAsset.hpp"

namespace Pine
{
    class AudioFile;

    class AudioSource final : public IComponent
    {
    private:
        AssetHandle<AudioFile> m_AudioFile;

        bool m_IsPlaying = false;
        bool m_PlayOnStart = false;
        bool m_Loop = false;

        float m_Position = 0.f;
        float m_Volume = 1.f;

        int m_AudioSourceObjectId;
    public:
        AudioSource();

        void Play();
        void Pause();
        void Stop();

        bool IsPlaying() const;
        float GetPosition() const;

        void SetPlayOnStart(bool playOnStart);
        bool GetPlayOnStart() const;

        void SetVolume(float volume);
        float GetVolume() const;

        void OnCreated() override;
        void OnCopied() override;

        void SetAudioFile(AudioFile* file);
        AudioFile* GetAudioFile() const;

        void OnUpdate(float deltaTime) override;
    };

}

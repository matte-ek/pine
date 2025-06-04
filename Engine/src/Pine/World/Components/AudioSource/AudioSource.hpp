#pragma once
#include <AL/al.h>
#include <glm/vec3.hpp>

#include "Pine/World/Components/IComponent/IComponent.hpp"
#include "Pine/Assets/IAsset/IAsset.hpp"

namespace Pine
{
    class AudioFile;

    class AudioSource final : public IComponent
    {
    private:
        AssetHandle<AudioFile> m_AudioFile;
        ALuint m_SourceId;

        bool m_IsPlaying = false;
        bool m_PlayOnStart = false;
        bool m_Loop = false;

        float m_PlaybackPosition = 0.f;
        float m_Volume = 1.f;

        glm::vec3 m_WorldPosition = glm::vec3(0.f);

        int m_AudioSourceObjectId;
    public:
        AudioSource();

        void Play() const;
        void Pause();
        void Stop();

        bool IsPlaying() const;
        float GetPlaybackPosition() const;

        glm::vec3 GetWorldPosition() const;
        void SetWorldPosition(glm::vec3 position) const;

        void SetPlayOnStart(bool playOnStart);
        bool GetPlayOnStart() const;

        void SetVolume(float volume) const;
        float GetVolume() const;

        void OnSetup() override;
        //void OnCopied() override;

        void SetAudioFile(AudioFile* file);
        AudioFile* GetAudioFile() const;
    };

}

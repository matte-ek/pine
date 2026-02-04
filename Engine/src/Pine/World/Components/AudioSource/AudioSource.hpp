#pragma once
#include <AL/al.h>
#include <glm/vec3.hpp>

#include "Pine/World/Components/Component/Component.hpp"
#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Core/Math/Math.hpp"

namespace Pine
{
    class AudioFile;

    class AudioSource final : public Component
    {
    private:
        AssetHandle<AudioFile> m_AudioFile;
        ALuint m_SourceId = 0;

        bool m_IsPlaying = false;
        bool m_PlayOnStart = false;
        bool m_Loop = false;

        float m_PlaybackPosition = 0.f;
        float m_Volume = 1.f;

        Vector3f m_WorldPosition = Vector3f(0.f);

        int m_AudioSourceObjectId = 0;
    public:
        AudioSource();

        void Play() const;
        void Pause();
        void Stop();

        bool IsPlaying() const;
        float GetPlaybackPosition() const;

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

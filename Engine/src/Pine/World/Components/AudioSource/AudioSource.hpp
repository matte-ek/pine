#pragma once

#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Audio/Audio.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"
#include "Pine/World/Components/Component/Component.hpp"

namespace Pine
{
    class AudioFile;

    // A sound emitter in the world. The component holds what the author sets - which clip, how
    // loud, whether it loops - plus the state it has been asked to be in, and nothing else: it
    // never touches the audio device. Pine::Audio reads all of this once a frame, lends the source
    // a voice for as long as it is making noise, and keeps the device matching.
    //
    // That is also why asking a source to play is a request rather than a promise. Voices are a
    // limited resource, and a source that asks for one while they are all busy is not heard.
    class AudioSource final : public Component
    {
    private:
        AssetHandle<AudioFile> m_AudioFile;

        Audio::PlaybackState m_PlaybackState = Audio::PlaybackState::Stopped;

        bool m_PlayOnStart = false;
        bool m_Loop = false;

        // A spatial source is heard from wherever its entity is and fades with distance from the
        // listener. Turn it off for music and UI sound, which should play at a constant volume
        // regardless of where the listener happens to be standing.
        //
        // Worth knowing: only mono clips are positioned - a stereo one plays flat wherever it is.
        // AudioImportConfiguration::ForceMono is how a stereo source file is made usable here.
        bool m_Spatial = true;

        float m_Volume = 1.f;
        float m_Pitch = 1.f;

        // Distance attenuation for a spatial source, in world units: full volume out to
        // m_ReferenceDistance, fading from there, and never quieter than it is at m_MaxDistance.
        // m_RolloffFactor scales how fast that fade happens, and 0 switches attenuation off.
        float m_ReferenceDistance = 1.f;
        float m_MaxDistance = 50.f;
        float m_RolloffFactor = 1.f;

        Audio::PlaybackHintData m_PlaybackHintData;

        struct AudioSourceSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_ASSET(AudioFile);
            PINE_SERIALIZE_PRIMITIVE(PlayOnStart, Serialization::DataType::Boolean);
            PINE_SERIALIZE_PRIMITIVE(Loop, Serialization::DataType::Boolean);
            PINE_SERIALIZE_PRIMITIVE(Spatial, Serialization::DataType::Boolean);
            PINE_SERIALIZE_PRIMITIVE(Volume, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(Pitch, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(ReferenceDistance, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(MaxDistance, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(RolloffFactor, Serialization::DataType::Float32);
        };
    public:
        AudioSource();

        void SetAudioFile(AudioFile* audioFile);
        AudioFile* GetAudioFile() const;

        // Start playing, or resume after a pause. Playing an already-playing source does nothing;
        // call AudioSource::Stop() first to start the clip over.
        void Play();

        // Hold playback where it is. AudioSource::Play() carries on from the same place.
        void Pause();

        // Stop and rewind, so the next AudioSource::Play() starts the clip from the beginning.
        void Stop();

        // What the source has been asked to do. Pine::Audio sets this back to Stopped when a clip
        // that is not looping reaches its end, so it stays true rather than only reflecting the
        // last call made.
        Audio::PlaybackState GetPlaybackState() const;
        bool IsPlaying() const;

        void SetPlayOnStart(bool playOnStart);
        bool GetPlayOnStart() const;

        void SetLoop(bool loop);
        bool GetLoop() const;

        void SetSpatial(bool spatial);
        bool GetSpatial() const;

        // Clamped to zero and up. There is no ceiling: above 1 a source is amplified, which will
        // clip if the mix was already loud.
        void SetVolume(float volume);
        float GetVolume() const;

        // Playback rate, which shifts pitch with it - 2 plays an octave up and twice as fast.
        // Clamped to a small positive value, since zero would mean playing nothing at all.
        void SetPitch(float pitch);
        float GetPitch() const;

        void SetReferenceDistance(float distance);
        float GetReferenceDistance() const;

        // Kept at or above the reference distance, so the fade can never run backwards.
        void SetMaxDistance(float distance);
        float GetMaxDistance() const;

        void SetRolloffFactor(float factor);
        float GetRolloffFactor() const;

        // How far into the clip playback has got, in seconds. Mirrored off the audio device once a
        // frame, so it holds whatever was last set until Pine::Audio has actually played some of
        // the clip. Seeking a source that holds no voice is fine - it takes effect when one is
        // lent, which is what makes "play this from halfway" a single call.
        void SetPlaybackPosition(float seconds);
        float GetPlaybackPosition() const;

        // Pine::Audio's own state for this source. Not for anything else to write to.
        Audio::PlaybackHintData& GetPlaybackHintData();

        void OnSetup() override;
        void OnCopied() override;
        void OnDestroyed() override;

        void LoadData(const ByteSpan& span) override;
        ByteSpan SaveData() override;
    };
}

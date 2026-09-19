#pragma once

#include "Pine/Core/Math/Math.hpp"

#include "IAudioBuffer.hpp"

namespace Pine::Audio
{
    // What a source is doing right now. A clip that has run off its end reports Stopped without
    // anything having asked it to stop, which is how Pine::Audio notices a one-shot has finished
    // and can hand its voice to something else.
    enum class PlaybackState
    {
        Stopped,
        Playing,
        Paused
    };

    // One voice on the audio device: somewhere a buffer can be played from, with its own position,
    // volume and play state. Owned by Pine::Audio, which lends voices out to AudioSource components
    // for as long as they are making noise - an audio device has far fewer of these than a level
    // has sounds, so nothing holds one permanently.
    class IAudioSource
    {
    public:
        virtual ~IAudioSource() = default;

        // The clip to play. Playback stops when the clip changes, since a position measured in the
        // old clip means nothing in the new one.
        virtual void SetBuffer(IAudioBuffer* buffer) = 0;

        virtual void Play() = 0;
        virtual void Pause() = 0;
        virtual void Stop() = 0;

        virtual PlaybackState GetState() const = 0;

        // A spatial source is heard from wherever it is in the world and fades with distance from
        // the listener. A non-spatial one plays at a fixed position on the listener, which is what
        // 2D audio - music, UI - is: there is no separate path for it.
        virtual void SetSpatial(bool spatial) = 0;
        virtual void SetPosition(const Vector3f& position) = 0;

        virtual void SetVolume(float volume) = 0;
        virtual void SetPitch(float pitch) = 0;
        virtual void SetLooping(bool loop) = 0;

        // How a spatial source fades with distance, in world units: full volume out to
        // 'referenceDistance', fading from there, and never quieter than it is at 'maxDistance'.
        // 'rolloffFactor' scales how fast that fade happens, and 0 switches attenuation off.
        virtual void SetAttenuation(float referenceDistance, float maxDistance, float rolloffFactor) = 0;

        virtual void SetPlaybackPosition(float seconds) = 0;
        virtual float GetPlaybackPosition() const = 0;

        virtual void Dispose() = 0;
    };
}

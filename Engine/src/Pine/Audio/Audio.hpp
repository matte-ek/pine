#pragma once

#include <cstdint>

#include "Pine/Audio/Interfaces/IAudioAPI.hpp"

namespace Pine
{
    class AudioFile;
    class AudioSource;
}

namespace Pine::Audio
{
    // Playback state that Pine::Audio owns and maintains on an AudioSource, reached through
    // AudioSource::GetPlaybackHintData(). It lives on the component for the same reason the
    // renderer's per-object state lives on a ModelRenderer: the subsystem needs somewhere with the
    // component's exact lifetime to keep it, and the component is the only thing that has one.
    //
    // Nothing outside Pine::Audio should write to this.
    struct PlaybackHintData
    {
        // Which voice this source has been lent, or -1 when it holds none. Voices are only handed
        // out while a source is actually making noise, so most sources hold none most of the time.
        int VoiceIndex = -1;

        // Bumped by the pool whenever a voice changes hands, so a handle left behind on a source
        // that has lost its voice cannot address whoever holds that voice now.
        std::uint32_t VoiceGeneration = 0;

        // How far into the clip playback has got, in seconds, mirrored off the audio device once a
        // frame. Doubles as the position a seek is aiming at: AudioSource::SetPlaybackPosition
        // writes it and raises SeekRequested, and Pine::Audio moves the voice to it and lowers the
        // flag again. A seek has to survive that gap because a source may be asked to seek at a
        // moment when it holds no voice at all.
        float PlaybackPosition = 0.f;
        bool SeekRequested = false;
    };

    // Creates and initializes the audio API, returns false if there is no usable output device.
    bool Setup();

    // Call before application exit, to free the audio device and everything it holds.
    void Shutdown();

    // Makes the audio device match the world: moves the listener to the active AudioListener, and
    // lends every AudioSource that wants to be heard a voice positioned and configured the way the
    // component says. Called once a frame from World::Update().
    void Update();

    bool HasInitializedAudioAPI();
    IAudioAPI* GetAudioAPI();

    // Auditions a clip outside the world, for the editor's asset panel. A preview plays flat on a
    // voice of its own, and keeps playing while the world is paused - which the editor's world is
    // outside play mode. While the world is paused it is also heard at full volume, listener or
    // not; in play mode it is heard through the game's listener like everything else. There is
    // one preview at a time, so starting another replaces it.
    void PlayPreview(AudioFile* clip);
    void StopPreview();

    // The clip being previewed, or nullptr once the preview has been stopped or reached its end.
    AudioFile* GetPreviewClip();

    // How far into the clip the preview has got, in seconds.
    float GetPreviewPosition();

    namespace Internal
    {
        // Hands a source's voice straight back, instead of waiting for the next Update() to notice
        // it is no longer wanted. AudioSource calls this as it is destroyed, so a sound ends with
        // its component rather than a frame later.
        void ReleaseVoice(AudioSource& source);

        // Stops the preview if it is playing 'clip' and unbinds the clip from its voice. OpenAL
        // refuses to rewrite or delete a buffer that a voice has bound, so AudioFile calls this
        // before doing either.
        void StopPreviewOf(const AudioFile& clip);
    }
}

using Pine.Assets;
using Pine.Core;
using Pine.Core.Bindings;

namespace Pine.World.Components
{
    // What a source has been asked to do. The engine sets this back to Stopped when a clip that is
    // not looping reaches its end, so it stays true rather than only reflecting the last call made.
    public enum PlaybackState
    {
        Stopped,
        Playing,
        Paused
    }

    // A sound emitter in the world. The component holds what the author sets - which clip, how
    // loud, whether it loops - plus the state it has been asked to be in, and the engine makes the
    // audio device match once a frame.
    //
    // That is why asking a source to play is a request rather than a promise: voices are a limited
    // resource, and a source that asks for one while they are all busy is simply not heard.
    [ComponentType(ComponentType.AudioSource)]
    public unsafe class AudioSource : Component
    {
        // The clip this source plays. Assigning null leaves the source with nothing to play.
        public Audio AudioFile
        {
            get => Interop.ObjectFrom<Audio>(ComponentBindings.AudioSourceGetAudioFile(InternalId));
            set => ComponentBindings.AudioSourceSetAudioFile(InternalId, value?.Id ?? default(UId));
        }

        public PlaybackState PlaybackState
            => (PlaybackState)ComponentBindings.AudioSourceGetPlaybackState(InternalId);

        public bool IsPlaying => ComponentBindings.AudioSourceIsPlaying(InternalId) != 0;

        // Whether the source starts playing as soon as it enters the world. Changing it afterwards
        // only affects the next time this source is set up, so it is an authoring switch rather
        // than a way to start a sound - call Play() for that.
        public bool PlayOnStart
        {
            get => ComponentBindings.AudioSourceGetPlayOnStart(InternalId) != 0;
            set => ComponentBindings.AudioSourceSetPlayOnStart(InternalId, value ? (byte)1 : (byte)0);
        }

        public bool Loop
        {
            get => ComponentBindings.AudioSourceGetLoop(InternalId) != 0;
            set => ComponentBindings.AudioSourceSetLoop(InternalId, value ? (byte)1 : (byte)0);
        }

        // A spatial source is heard from wherever its entity is and fades with distance from the
        // listener. Turn it off for music and UI sound, which should play at a constant volume.
        //
        // Worth knowing: only mono clips are positioned - a stereo one plays flat wherever it is.
        public bool Spatial
        {
            get => ComponentBindings.AudioSourceGetSpatial(InternalId) != 0;
            set => ComponentBindings.AudioSourceSetSpatial(InternalId, value ? (byte)1 : (byte)0);
        }

        // Clamped to zero and up. There is no ceiling: above 1 a source is amplified, which will
        // clip if the mix was already loud.
        public float Volume
        {
            get => ComponentBindings.AudioSourceGetVolume(InternalId);
            set => ComponentBindings.AudioSourceSetVolume(InternalId, value);
        }

        // Playback rate, which shifts pitch with it - 2 plays an octave up and twice as fast.
        // Clamped to a small positive value, since zero would mean playing nothing at all.
        public float Pitch
        {
            get => ComponentBindings.AudioSourceGetPitch(InternalId);
            set => ComponentBindings.AudioSourceSetPitch(InternalId, value);
        }

        // Distance attenuation for a spatial source, in world units: full volume out to
        // ReferenceDistance, fading from there, and never quieter than it is at MaxDistance.
        // RolloffFactor scales how fast that fade happens, and 0 switches attenuation off.
        public float ReferenceDistance
        {
            get => ComponentBindings.AudioSourceGetReferenceDistance(InternalId);
            set => ComponentBindings.AudioSourceSetReferenceDistance(InternalId, value);
        }

        // Kept at or above ReferenceDistance, so the fade can never run backwards.
        public float MaxDistance
        {
            get => ComponentBindings.AudioSourceGetMaxDistance(InternalId);
            set => ComponentBindings.AudioSourceSetMaxDistance(InternalId, value);
        }

        public float RolloffFactor
        {
            get => ComponentBindings.AudioSourceGetRolloffFactor(InternalId);
            set => ComponentBindings.AudioSourceSetRolloffFactor(InternalId, value);
        }

        // How far into the clip playback has got, in seconds. Mirrored off the audio device once a
        // frame, so it holds whatever was last assigned until the engine has actually played some
        // of the clip. Seeking a source that is not playing is fine - it takes effect when it is,
        // which is what makes "play this from halfway" two lines.
        public float PlaybackPosition
        {
            get => ComponentBindings.AudioSourceGetPlaybackPosition(InternalId);
            set => ComponentBindings.AudioSourceSetPlaybackPosition(InternalId, value);
        }

        // Start playing, or resume after a pause. Playing an already-playing source does nothing;
        // call Stop() first to start the clip over.
        public void Play() => ComponentBindings.AudioSourcePlay(InternalId);

        // Hold playback where it is. Play() carries on from the same place.
        public void Pause() => ComponentBindings.AudioSourcePause(InternalId);

        // Stop and rewind, so the next Play() starts the clip from the beginning.
        public void Stop() => ComponentBindings.AudioSourceStop(InternalId);
    }
}

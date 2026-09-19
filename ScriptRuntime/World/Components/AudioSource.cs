using System.Runtime.CompilerServices;
using Pine.Assets;
using Pine.Core;

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
    public class AudioSource : Component
    {
        // The clip this source plays. Assigning null leaves the source with nothing to play.
        public Audio AudioFile
        {
            get => (Audio)PineGetAudioFile(InternalId);
            set => PineSetAudioFile(InternalId, value?.Id ?? default(UId));
        }

        public PlaybackState PlaybackState => (PlaybackState)PineGetPlaybackState(InternalId);

        public bool IsPlaying => PineIsPlaying(InternalId);

        // Whether the source starts playing as soon as it enters the world. Changing it afterwards
        // only affects the next time this source is set up, so it is an authoring switch rather
        // than a way to start a sound - call Play() for that.
        public bool PlayOnStart
        {
            get => PineGetPlayOnStart(InternalId);
            set => PineSetPlayOnStart(InternalId, value);
        }

        public bool Loop
        {
            get => PineGetLoop(InternalId);
            set => PineSetLoop(InternalId, value);
        }

        // A spatial source is heard from wherever its entity is and fades with distance from the
        // listener. Turn it off for music and UI sound, which should play at a constant volume.
        //
        // Worth knowing: only mono clips are positioned - a stereo one plays flat wherever it is.
        public bool Spatial
        {
            get => PineGetSpatial(InternalId);
            set => PineSetSpatial(InternalId, value);
        }

        // Clamped to zero and up. There is no ceiling: above 1 a source is amplified, which will
        // clip if the mix was already loud.
        public float Volume
        {
            get => PineGetVolume(InternalId);
            set => PineSetVolume(InternalId, value);
        }

        // Playback rate, which shifts pitch with it - 2 plays an octave up and twice as fast.
        // Clamped to a small positive value, since zero would mean playing nothing at all.
        public float Pitch
        {
            get => PineGetPitch(InternalId);
            set => PineSetPitch(InternalId, value);
        }

        // Distance attenuation for a spatial source, in world units: full volume out to
        // ReferenceDistance, fading from there, and never quieter than it is at MaxDistance.
        // RolloffFactor scales how fast that fade happens, and 0 switches attenuation off.
        public float ReferenceDistance
        {
            get => PineGetReferenceDistance(InternalId);
            set => PineSetReferenceDistance(InternalId, value);
        }

        // Kept at or above ReferenceDistance, so the fade can never run backwards.
        public float MaxDistance
        {
            get => PineGetMaxDistance(InternalId);
            set => PineSetMaxDistance(InternalId, value);
        }

        public float RolloffFactor
        {
            get => PineGetRolloffFactor(InternalId);
            set => PineSetRolloffFactor(InternalId, value);
        }

        // How far into the clip playback has got, in seconds. Mirrored off the audio device once a
        // frame, so it holds whatever was last assigned until the engine has actually played some
        // of the clip. Seeking a source that is not playing is fine - it takes effect when it is,
        // which is what makes "play this from halfway" two lines.
        public float PlaybackPosition
        {
            get => PineGetPlaybackPosition(InternalId);
            set => PineSetPlaybackPosition(InternalId, value);
        }

        // Start playing, or resume after a pause. Playing an already-playing source does nothing;
        // call Stop() first to start the clip over.
        public void Play() => PinePlay(InternalId);

        // Hold playback where it is. Play() carries on from the same place.
        public void Pause() => PinePause(InternalId);

        // Stop and rewind, so the next Play() starts the clip from the beginning.
        public void Stop() => PineStop(InternalId);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Asset PineGetAudioFile(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetAudioFile(uint id, UId assetId);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PinePlay(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PinePause(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineStop(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int PineGetPlaybackState(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool PineIsPlaying(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool PineGetPlayOnStart(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetPlayOnStart(uint id, bool playOnStart);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool PineGetLoop(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetLoop(uint id, bool loop);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool PineGetSpatial(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetSpatial(uint id, bool spatial);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float PineGetVolume(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetVolume(uint id, float volume);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float PineGetPitch(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetPitch(uint id, float pitch);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float PineGetReferenceDistance(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetReferenceDistance(uint id, float distance);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float PineGetMaxDistance(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetMaxDistance(uint id, float distance);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float PineGetRolloffFactor(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetRolloffFactor(uint id, float factor);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float PineGetPlaybackPosition(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetPlaybackPosition(uint id, float seconds);
    }
}

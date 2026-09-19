using System.Runtime.CompilerServices;

namespace Pine.World.Components
{
    // The ears of the world: every spatial AudioSource is heard from this entity's transform, and
    // fades with its distance from it.
    //
    // A device has exactly one listener, so the first enabled AudioListener in the world is the one
    // that counts. With none at all, nothing is audible: there is nobody there to hear it.
    public class AudioListener : Component
    {
        // Master volume, applied on top of whatever each source is set to. Clamped to zero and up,
        // with no ceiling - above 1 the whole mix is amplified.
        public float Volume
        {
            get => PineGetVolume(InternalId);
            set => PineSetVolume(InternalId, value);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float PineGetVolume(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetVolume(uint id, float volume);
    }
}

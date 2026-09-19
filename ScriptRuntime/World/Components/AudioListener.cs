using Pine.Core.Bindings;

namespace Pine.World.Components
{
    // The ears of the world: every spatial AudioSource is heard from this entity's transform, and
    // fades with its distance from it.
    //
    // A device has exactly one listener, so the first enabled AudioListener in the world is the one
    // that counts. With none at all, nothing is audible: there is nobody there to hear it.
    [ComponentType(ComponentType.AudioListener)]
    public unsafe class AudioListener : Component
    {
        // Master volume, applied on top of whatever each source is set to. Clamped to zero and up,
        // with no ceiling - above 1 the whole mix is amplified.
        public float Volume
        {
            get => ComponentBindings.AudioListenerGetVolume(InternalId);
            set => ComponentBindings.AudioListenerSetVolume(InternalId, value);
        }
    }
}

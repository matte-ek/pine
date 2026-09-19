using Pine.Core.Bindings;

namespace Pine.Assets
{
    // A decoded audio clip, played by an AudioSource. Named after AssetType.Audio rather than the
    // native Pine::AudioFile: the object factory resolves a managed asset class by the asset type's
    // own name, so this is the name that makes a clip reachable from C# at all.
    public unsafe class Audio : Asset
    {
        // How long the clip runs, in seconds. Worth having next to AudioSource.PlaybackPosition,
        // which is measured on the same scale.
        public float Duration => AssetBindings.AudioGetDuration(Id);
    }
}

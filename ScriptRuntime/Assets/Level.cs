using Pine.Core;
using Pine.Core.Bindings;

namespace Pine.Assets
{
    public unsafe class Level : Asset
    {
        private LevelRenderingSettings _rendering;

        // The level the world is currently showing.
        public static Level Active => Interop.ObjectFrom<Level>(AssetBindings.LevelGetActive());

        // Fog, ambient light and the post-processing look this level is drawn with.
        public LevelRenderingSettings Rendering => _rendering ??= new LevelRenderingSettings(this);

        public void CreateFromWorld() => AssetBindings.LevelCreateFromWorld(Id);
        public void Load() => AssetBindings.LevelLoad(Id);
    }
}

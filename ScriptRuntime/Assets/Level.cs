using Pine.Core.Bindings;

namespace Pine.Assets
{
    public unsafe class Level : Asset
    {
        public void CreateFromWorld() => AssetBindings.LevelCreateFromWorld(Id);
        public void Load() => AssetBindings.LevelLoad(Id);
    }
}

using System.Runtime.CompilerServices;

namespace Pine.Assets
{
    public class Level : Asset
    {
        public void CreateFromWorld() => CreateFromWorld(_internalId);
        public void Load() => Load(_internalId);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void CreateFromWorld(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Load(uint id);
    }
}
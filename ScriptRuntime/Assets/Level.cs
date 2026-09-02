using System.Runtime.CompilerServices;
using Pine.Core;

namespace Pine.Assets
{
    public class Level : Asset
    {
        public void CreateFromWorld() => CreateFromWorld(Id);
        public void Load() => Load(Id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void CreateFromWorld(UId id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Load(UId id);
    }
}

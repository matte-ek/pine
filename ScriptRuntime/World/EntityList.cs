using System.Runtime.CompilerServices;

namespace Pine.World
{
    public static class EntityList
    {
        public static Entity Create() => Entity.Create();
        public static Entity Create(string name) => Entity.Create(name);

        public static Entity Find(string name) => FindByName(name);
        
        public static Entity[] Find(ulong tag) => FindByTag(tag);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern Entity[] GetAll();
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Entity FindByName(string name);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Entity[] FindByTag(ulong tag);
    }
}
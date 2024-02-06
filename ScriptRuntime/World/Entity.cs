using System.Runtime.CompilerServices;
using Pine.World.Components;

namespace Pine.World
{
    public class Entity
    {
        public readonly uint Id =  0;
        private readonly int _internalId = 0;
    
        public string Name
        {
            get => GetName(_internalId);
            set => SetName(_internalId, value);
        }
    
        protected Entity()
        {
        }

        public Transform Transform => GetTransform(_internalId);
    
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern string GetName(int id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetName(int id, string name);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Transform GetTransform(int id);
    }
}


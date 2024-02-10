using System;
using System.Runtime.CompilerServices;
using Pine.World.Components;

namespace Pine.World
{
    public class Entity
    {
        public readonly uint Id =  0;
        internal readonly uint _internalId = 0;
    
        public string Name
        {
            get => GetName(_internalId);
            set => SetName(_internalId, value);
        }
    
        public bool Active
        {
            get => GetActive(_internalId);
            set => SetActive(_internalId, value);
        }
        
        public bool Static
        {
            get => GetStatic(_internalId);
            set => SetStatic(_internalId, value);
        }
        
        protected Entity()
        {
        }

        public Transform Transform => GetTransform(_internalId);
        
        public bool HasComponent<T>() where T: Component => HasComponent(_internalId, typeof(T));
        public T AddComponent<T>() where T : Component => (T)AddComponent(_internalId, typeof(T));
        public T GetComponent<T>() where T : Component => (T)GetComponent(_internalId, typeof(T));

        public static Entity Create() => CreateEntity("");
        public static Entity Create(string name) => CreateEntity(name);
    
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern string GetName(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetName(uint id, string name);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool GetActive(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetActive(uint id, bool active);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool GetStatic(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetStatic(uint id, bool value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Transform GetTransform(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool HasComponent(uint id, Type type);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Component AddComponent(uint id, Type type);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Component GetComponent(uint id, Type type);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Entity CreateEntity(string name);

    }
}


using System;
using System.Runtime.CompilerServices;
using Pine.Core;
using Pine.World.Components;

namespace Pine.World
{
    public class Entity
    {
        public readonly uint Id = 0;
        public bool IsValid => _isValid == 1;
        
        public string Name
        {
            get => GetName(InternalId);
            set => SetName(InternalId, value);
        }
    
        public bool Active
        {
            get => GetActive(InternalId);
            set => SetActive(InternalId, value);
        }
        
        public bool Static
        {
            get => GetStatic(InternalId);
            set => SetStatic(InternalId, value);
        }
        
        public void Destroy() => DestroyEntity(_internalId);
        
        public Transform Transform => GetTransform(InternalId);
        
        public bool HasComponent<T>() where T: Component => HasComponent(InternalId, typeof(T));
        public T AddComponent<T>() where T : Component => (T)AddComponent(InternalId, typeof(T));
        public T GetComponent<T>() where T : Component => (T)GetComponent(InternalId, typeof(T));
        
        internal uint InternalId
        {
            get
            {
                if (_isValid == 0)
                {
                    Log.Error("Attempt to access invalid entity");
                    return uint.MaxValue;
                }
                
                return _internalId;
            }
        }
        
        protected Entity()
        {
        }

        private uint _internalId = 0;
        private int _isValid = 1;
        
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
        private static extern void DestroyEntity(uint id);
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


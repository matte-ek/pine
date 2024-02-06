using System.Runtime.CompilerServices;

namespace Pine.World
{
    public enum ComponentType
    {
        Transform = 0,
        ModelRenderer,
        TerrainRenderer,
        Camera,
        Light,
        Collider,
        RigidBody,
        Collider2D,
        RigidBody2D,
        SpriteRenderer,
        TilemapRenderer,
        NativeScript,
        Script
    }
    
    public class Component
    {
        public readonly Entity Parent;
        public readonly ComponentType ComponentType;
        
        protected int _internalId = 0;
        
        public bool Active
        {
            get => GetActive(_internalId, (int)ComponentType);
            set => SetActive(_internalId, (int)ComponentType, value);
        }
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool GetActive(int id, int type);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetActive(int id, int type, bool active);
    }
}
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
        public readonly ComponentType Type;
        
        protected uint _internalId = 0;
        
        public bool Active
        {
            get => GetActive(_internalId, (int)Type);
            set => SetActive(_internalId, (int)Type, value);
        }
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool GetActive(uint id, int type);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetActive(uint id, int type, bool active);
    }
}
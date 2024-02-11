using System.Runtime.CompilerServices;
using Pine.Core;

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
        
        public bool Active
        {
            get => GetActive(InternalId, (int)Type);
            set => SetActive(InternalId, (int)Type, value);
        }
        
        internal uint InternalId
        {
            get
            {
                if (!_isValid)
                {
                    Log.Error("Attempt to access invalid component");
                    return uint.MaxValue;
                }
                
                return _internalId;
            }
        } 
        
        private uint _internalId = 0;
        private bool _isValid = true;
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool GetActive(uint id, int type);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetActive(uint id, int type, bool active);
    }
}
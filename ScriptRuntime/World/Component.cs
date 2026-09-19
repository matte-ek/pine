using Pine.Core;
using Pine.Core.Bindings;

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
        Script,
        AudioSource,
        AudioListener,
        CharacterController
    }

    public unsafe class Component
    {
        public readonly Entity Parent;
        public readonly ComponentType Type;

        public bool Active
        {
            get => ComponentBindings.GetActive(InternalId, (int)Type) != 0;
            set => ComponentBindings.SetActive(InternalId, (int)Type, value ? (byte)1 : (byte)0);
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
    }
}

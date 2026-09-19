using Pine.Core.Bindings;
using Pine.Math;

namespace Pine.World.Components
{
    public enum ColliderType
    {
        Box,
        Sphere,
        Capsule,
        ConvexMesh,
        ConcaveMesh,
        HeightField
    }

    [ComponentType(ComponentType.Collider)]
    public unsafe class Collider : Component
    {
        public ColliderType ColliderType
        {
            get => (ColliderType)ComponentBindings.ColliderGetColliderType(InternalId);
            set => ComponentBindings.ColliderSetColliderType(InternalId, (int)value);
        }

        // The collider's offset from the entity's origin.
        public Vector3 Position
        {
            get
            {
                Vector3 position;

                ComponentBindings.ColliderGetPosition(InternalId, &position);

                return position;
            }
            set => ComponentBindings.ColliderSetPosition(InternalId, &value);
        }

        // Box half-extents. Sphere and capsule read their dimensions out of this too - use Radius
        // and Height for those rather than working out which component they live in.
        public Vector3 Size
        {
            get
            {
                Vector3 size;

                ComponentBindings.ColliderGetSize(InternalId, &size);

                return size;
            }
            set => ComponentBindings.ColliderSetSize(InternalId, &value);
        }

        // Sphere and capsule only.
        public float Radius
        {
            get => ComponentBindings.ColliderGetRadius(InternalId);
            set => ComponentBindings.ColliderSetRadius(InternalId, value);
        }

        // Capsule only.
        public float Height
        {
            get => ComponentBindings.ColliderGetHeight(InternalId);
            set => ComponentBindings.ColliderSetHeight(InternalId, value);
        }

        // The layer this collider occupies, and the mask of layers it collides with. The same layer
        // space CharacterController and Physics3D.RayCast use.
        public uint Layer
        {
            get => ComponentBindings.ColliderGetLayer(InternalId);
            set => ComponentBindings.ColliderSetLayer(InternalId, value);
        }

        public uint LayerMask
        {
            get => ComponentBindings.ColliderGetLayerMask(InternalId);
            set => ComponentBindings.ColliderSetLayerMask(InternalId, value);
        }

        // A trigger is still reported by queries but no longer stops anything moving through it.
        // Note there are no enter/exit callbacks yet - poll with Physics3D.RayCast or a distance
        // check instead.
        public bool IsTrigger
        {
            get => ComponentBindings.ColliderGetIsTrigger(InternalId) != 0;
            set => ComponentBindings.ColliderSetIsTrigger(InternalId, value ? (byte)1 : (byte)0);
        }

        public uint TriggerMask
        {
            get => ComponentBindings.ColliderGetTriggerMask(InternalId);
            set => ComponentBindings.ColliderSetTriggerMask(InternalId, value);
        }
    }
}

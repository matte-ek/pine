using System.Runtime.CompilerServices;
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

    public class Collider : Component
    {
        public ColliderType ColliderType
        {
            get => (ColliderType)PineGetColliderType(InternalId);
            set => PineSetColliderType(InternalId, (int)value);
        }

        // The collider's offset from the entity's origin.
        public Vector3 Position
        {
            get
            {
                PineGetPosition(InternalId, out var position);
                return position;
            }
            set => PineSetPosition(InternalId, ref value);
        }

        // Box half-extents. Sphere and capsule read their dimensions out of this too - use Radius
        // and Height for those rather than working out which component they live in.
        public Vector3 Size
        {
            get
            {
                PineGetSize(InternalId, out var size);
                return size;
            }
            set => PineSetSize(InternalId, ref value);
        }

        // Sphere and capsule only.
        public float Radius
        {
            get => PineGetRadius(InternalId);
            set => PineSetRadius(InternalId, value);
        }

        // Capsule only.
        public float Height
        {
            get => PineGetHeight(InternalId);
            set => PineSetHeight(InternalId, value);
        }

        // The layer this collider occupies, and the mask of layers it collides with. The same layer
        // space CharacterController and Physics3D.RayCast use.
        public uint Layer
        {
            get => PineGetLayer(InternalId);
            set => PineSetLayer(InternalId, value);
        }

        public uint LayerMask
        {
            get => PineGetLayerMask(InternalId);
            set => PineSetLayerMask(InternalId, value);
        }

        // A trigger is still reported by queries but no longer stops anything moving through it.
        // Note there are no enter/exit callbacks yet - poll with Physics3D.RayCast or a distance
        // check instead.
        public bool IsTrigger
        {
            get => PineGetIsTrigger(InternalId);
            set => PineSetIsTrigger(InternalId, value);
        }

        public uint TriggerMask
        {
            get => PineGetTriggerMask(InternalId);
            set => PineSetTriggerMask(InternalId, value);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int PineGetColliderType(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetColliderType(uint id, int type);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineGetPosition(uint id, out Vector3 position);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetPosition(uint id, ref Vector3 position);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineGetSize(uint id, out Vector3 size);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetSize(uint id, ref Vector3 size);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float PineGetRadius(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetRadius(uint id, float radius);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float PineGetHeight(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetHeight(uint id, float height);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern uint PineGetLayer(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetLayer(uint id, uint layer);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern uint PineGetLayerMask(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetLayerMask(uint id, uint layerMask);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool PineGetIsTrigger(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetIsTrigger(uint id, bool isTrigger);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern uint PineGetTriggerMask(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetTriggerMask(uint id, uint mask);
    }
}

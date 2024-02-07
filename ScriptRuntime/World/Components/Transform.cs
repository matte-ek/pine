using System.Runtime.CompilerServices;
using Pine.Math;

namespace Pine.World.Components
{
    public class Transform : Component
    {
        public Vector3 Position
        {
            get
            {
                GetLocalPosition(_internalId, out var position);
                return position;
            }
            set
            {
                SetLocalPosition(_internalId, ref value);
            }
        }
        
        public Quaternion Rotation
        {
            get
            {
                GetLocalRotation(_internalId, out var rotation);
                return rotation;
            }
            set
            {
                SetLocalRotation(_internalId, ref value);
            }
        }
        
        public Vector3 Scale
        {
            get
            {
                GetLocalScale(_internalId, out var scale);
                return scale;
            }
            set
            {
                SetLocalScale(_internalId, ref value);
            }
        }
        
        public Vector3 Up
        {
            get
            {
                GetUp(_internalId, out var up);
                return up;
            }
        }
        
        public Vector3 Right
        {
            get
            {
                GetRight(_internalId, out var right);
                return right;
            }
        }
        
        public Vector3 Forward
        {
            get
            {
                GetForward(_internalId, out var forward);
                return forward;
            }
        }
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetLocalPosition(uint id, ref Vector3 position);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetLocalPosition(uint id, out Vector3 position);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetLocalRotation(uint id, ref Quaternion rotation);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetLocalRotation(uint id, out Quaternion rotation);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetLocalScale(uint id, ref Vector3 scale);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetLocalScale(uint id, out Vector3 scale);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetUp(uint id, out Vector3 up);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetRight(uint id, out Vector3 right);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetForward(uint id, out Vector3 forward);
    }
}
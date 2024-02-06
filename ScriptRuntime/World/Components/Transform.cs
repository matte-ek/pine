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
        private static extern void SetLocalPosition(int id, ref Vector3 position);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetLocalPosition(int id, out Vector3 position);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetLocalRotation(int id, ref Quaternion rotation);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetLocalRotation(int id, out Quaternion rotation);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetLocalScale(int id, ref Vector3 scale);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetLocalScale(int id, out Vector3 scale);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetUp(int id, out Vector3 up);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetRight(int id, out Vector3 right);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetForward(int id, out Vector3 forward);
    }
}
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
                GetPosition(InternalId, out var position);
                return position;
            }
        }
        
        public Vector3 Rotation
        {
            get
            {
                GetRotation(InternalId, out var rotation);
                return rotation;
            }
        }
        
        public Vector3 Scale
        {
            get
            {
                GetScale(InternalId, out var scale);
                return scale;
            }
        }
        
        public Vector3 LocalPosition
        {
            get
            {
                GetLocalPosition(InternalId, out var position);
                return position;
            }
            set
            {
                SetLocalPosition(InternalId, ref value);
            }
        }
        
        public Quaternion LocalRotation
        {
            get
            {
                GetLocalRotation(InternalId, out var rotation);
                return rotation;
            }
            set
            {
                SetLocalRotation(InternalId, ref value);
            }
        }
        
        public Vector3 LocalScale
        {
            get
            {
                GetLocalScale(InternalId, out var scale);
                return scale;
            }
            set
            {
                SetLocalScale(InternalId, ref value);
            }
        }
        
        public Vector3 Up
        {
            get
            {
                GetUp(InternalId, out var up);
                return up;
            }
        }
        
        public Vector3 Right
        {
            get
            {
                GetRight(InternalId, out var right);
                return right;
            }
        }
        
        public Vector3 Forward
        {
            get
            {
                GetForward(InternalId, out var forward);
                return forward;
            }
        }
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetPosition(uint id, out Vector3 position);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetRotation(uint id, out Vector3 position);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetScale(uint id, out Vector3 position);
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
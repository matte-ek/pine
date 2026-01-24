using System.Runtime.CompilerServices;
using Pine.Math;
using Pine.Physics.Data;

namespace Pine.Physics
{
    public static class Physics3D
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern RayCastHit[] RayCast(Vector3 origin, Vector3 direction, float maxDistance, int layerMask);
    }
}
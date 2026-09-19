using Pine.Math;

namespace Pine.Core.Bindings
{
    // The engine functions behind Pine.Physics.
    internal static unsafe class PhysicsBindings
    {
        public static delegate* unmanaged<Vector3, Vector3, float, int, int> RayCastQuery;
        public static delegate* unmanaged<int, void*, void> RayCastGetHit;

        public static void Bind()
        {
            RayCastQuery = (delegate* unmanaged<Vector3, Vector3, float, int, int>)
                Interop.Resolve("Pine.Physics.Physics3D::RayCastQuery");
            RayCastGetHit = (delegate* unmanaged<int, void*, void>)
                Interop.Resolve("Pine.Physics.Physics3D::RayCastGetHit");
        }
    }
}

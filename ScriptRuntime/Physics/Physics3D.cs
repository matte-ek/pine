using System.Runtime.InteropServices;
using Pine.Core;
using Pine.Core.Bindings;
using Pine.Math;
using Pine.Physics.Data;
using Pine.World;

namespace Pine.Physics
{
    public static class Physics3D
    {
        public static unsafe RayCastHit[] RayCast(Vector3 origin, Vector3 direction, float maxDistance, int layerMask)
        {
            // The query runs once, in the call that reports how many hits it found; the hits
            // themselves are then read out one at a time. The count has to come first because the
            // engine cannot allocate the array this returns, and nothing knows its length until
            // the query has run.
            var hits = new RayCastHit[PhysicsBindings.RayCastQuery(origin, direction, maxDistance, layerMask)];

            for (var index = 0; index < hits.Length; index++)
            {
                RayCastHitRaw hit;

                PhysicsBindings.RayCastGetHit(index, &hit);

                hits[index] = new RayCastHit
                {
                    Entity = Interop.ObjectFrom<Entity>(hit.EntityHandle),
                    Position = hit.Position,
                    Normal = hit.Normal
                };
            }

            return hits;
        }

        // How one hit crosses the boundary. RayCastHit itself cannot: its Entity field is a managed
        // reference, which the engine has no way to write, so the handle travels instead and
        // RayCast turns it back into the entity.
        [StructLayout(LayoutKind.Sequential)]
        private struct RayCastHitRaw
        {
            public ulong EntityHandle;
            public Vector3 Position;
            public Vector3 Normal;
        }
    }
}

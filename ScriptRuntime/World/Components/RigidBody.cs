using System.Runtime.CompilerServices;
using Pine.Math;

namespace Pine.World.Components
{
    public enum ForceType
    {
        Force,
        Impulsive,
        VelocityChange,
        Acceleration
    }
    
    public class RigidBody : Component
    {
        public void ApplyForce(Vector3 force, ForceType type = ForceType.Force) => ApplyForce(InternalId, ref force, type);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void ApplyForce(uint id, ref Vector3 position, ForceType type);
    }
}
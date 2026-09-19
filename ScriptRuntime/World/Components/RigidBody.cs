using Pine.Core.Bindings;
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

    [ComponentType(ComponentType.RigidBody)]
    public unsafe class RigidBody : Component
    {
        public void ApplyForce(Vector3 force, ForceType type = ForceType.Force)
            => ComponentBindings.RigidBodyApplyForce(InternalId, &force, (int)type);
    }
}

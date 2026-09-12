using System.Runtime.CompilerServices;
using Pine.Math;

namespace Pine.World.Components
{
    // A kinematic capsule character controller (backed by PhysX). Collides and slides against the
    // world instead of pushing through it. Drive it from a script by calling Move() each frame.
    public class CharacterController : Component
    {
        // True when the controller is resting on the ground this tick.
        public bool IsGrounded => PineIsGrounded(InternalId);

        // Move the controller by a world-space displacement (collide-and-slide). This is a
        // displacement, not a velocity - multiply your speed by delta time before passing it in.
        // Gravity is applied internally, so you only supply the intended horizontal movement.
        public void Move(Vector3 motion) => PineMove(InternalId, ref motion);

        // Teleport the controller to a world-space position (its feet). Setting the entity's
        // Transform directly does not work - the controller overwrites it every physics tick.
        // Any queued movement and the accumulated fall speed are cleared.
        public void SetPosition(Vector3 position) => PineSetPosition(InternalId, ref position);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineMove(uint id, ref Vector3 motion);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetPosition(uint id, ref Vector3 position);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool PineIsGrounded(uint id);
    }
}

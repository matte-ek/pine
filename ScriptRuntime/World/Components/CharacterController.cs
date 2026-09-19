using Pine.Core.Bindings;
using Pine.Math;

namespace Pine.World.Components
{
    // A kinematic capsule character controller (backed by PhysX). Collides and slides against the
    // world instead of pushing through it. Drive it from a script by calling Move() each frame.
    [ComponentType(ComponentType.CharacterController)]
    public unsafe class CharacterController : Component
    {
        // True when the controller is resting on the ground this tick.
        public bool IsGrounded => ComponentBindings.CharacterControllerIsGrounded(InternalId) != 0;

        // True when the controller ended the last tick pressed against something to its side.
        // If you integrate your own velocity, check this: a blocked tick travels less than it was
        // asked to, so keeping the full speed would bank it against the wall and release it the
        // moment you turn away. Fall back to Velocity when it is set.
        public bool IsTouchingSides => ComponentBindings.CharacterControllerIsTouchingSides(InternalId) != 0;

        // The velocity the controller actually achieved on the last physics tick. This is not the
        // motion you requested - walls, slope limits and step-ups all make the two differ.
        public Vector3 Velocity
        {
            get
            {
                Vector3 velocity;

                ComponentBindings.CharacterControllerGetVelocity(InternalId, &velocity);

                return velocity;
            }
        }

        // The vertical speed carried between ticks. Gravity accumulates into it, and landing or
        // hitting a ceiling clears it. Assign to it to jump - you decide when that is allowed,
        // typically only while IsGrounded.
        public float VerticalVelocity
        {
            get => ComponentBindings.CharacterControllerGetVerticalVelocity(InternalId);
            set => ComponentBindings.CharacterControllerSetVerticalVelocity(InternalId, value);
        }

        // Move the controller by a world-space displacement (collide-and-slide). This is a
        // displacement, not a velocity - multiply your speed by delta time before passing it in.
        // Gravity is applied internally, so you only supply the intended horizontal movement.
        public void Move(Vector3 motion) => ComponentBindings.CharacterControllerMove(InternalId, &motion);

        // Teleport the controller to a world-space position (its feet). Setting the entity's
        // Transform directly does not work - the controller overwrites it every physics tick.
        // Any queued movement and the accumulated fall speed are cleared.
        public void SetPosition(Vector3 position) => ComponentBindings.CharacterControllerSetPosition(InternalId, &position);
    }
}

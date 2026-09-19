using Pine.Core.Bindings;
using Pine.Math;

namespace Pine.World.Components
{
    [ComponentType(ComponentType.Transform)]
    public unsafe class Transform : Component
    {
        public Vector3 Position
        {
            get
            {
                Vector3 position;

                ComponentBindings.TransformGetPosition(InternalId, &position);

                return position;
            }
        }

        public Quaternion Rotation
        {
            get
            {
                Quaternion rotation;

                ComponentBindings.TransformGetRotation(InternalId, &rotation);

                return rotation;
            }
        }

        public Vector3 Scale
        {
            get
            {
                Vector3 scale;

                ComponentBindings.TransformGetScale(InternalId, &scale);

                return scale;
            }
        }

        public Vector3 LocalPosition
        {
            get
            {
                Vector3 position;

                ComponentBindings.TransformGetLocalPosition(InternalId, &position);

                return position;
            }
            set => ComponentBindings.TransformSetLocalPosition(InternalId, &value);
        }

        public Quaternion LocalRotation
        {
            get
            {
                Quaternion rotation;

                ComponentBindings.TransformGetLocalRotation(InternalId, &rotation);

                return rotation;
            }
            set => ComponentBindings.TransformSetLocalRotation(InternalId, &value);
        }

        public Vector3 LocalEulerAngles
        {
            get
            {
                Vector3 angles;

                ComponentBindings.TransformGetLocalEulerAngles(InternalId, &angles);

                return angles;
            }
            set => ComponentBindings.TransformSetLocalEulerAngles(InternalId, &value);
        }

        public Vector3 LocalScale
        {
            get
            {
                Vector3 scale;

                ComponentBindings.TransformGetLocalScale(InternalId, &scale);

                return scale;
            }
            set => ComponentBindings.TransformSetLocalScale(InternalId, &value);
        }

        public Vector3 Up
        {
            get
            {
                Vector3 up;

                ComponentBindings.TransformGetUp(InternalId, &up);

                return up;
            }
        }

        public Vector3 Right
        {
            get
            {
                Vector3 right;

                ComponentBindings.TransformGetRight(InternalId, &right);

                return right;
            }
        }

        public Vector3 Forward
        {
            get
            {
                Vector3 forward;

                ComponentBindings.TransformGetForward(InternalId, &forward);

                return forward;
            }
        }
    }
}

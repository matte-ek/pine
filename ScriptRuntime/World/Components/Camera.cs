using Pine.Core.Bindings;
using Pine.Math;

namespace Pine.World.Components
{
    public enum CameraType
    {
        Perspective,
        Orthographic
    }

    [ComponentType(ComponentType.Camera)]
    public unsafe class Camera : Component
    {
        public CameraType CameraType
        {
            get => (CameraType)ComponentBindings.CameraGetCameraType(InternalId);
            set => ComponentBindings.CameraSetCameraType(InternalId, (int)value);
        }

        public float NearPlane
        {
            get => ComponentBindings.CameraGetNearPlane(InternalId);
            set => ComponentBindings.CameraSetNearPlane(InternalId, value);
        }

        public float FarPlane
        {
            get => ComponentBindings.CameraGetFarPlane(InternalId);
            set => ComponentBindings.CameraSetFarPlane(InternalId, value);
        }

        // Vertical field of view in degrees. Only used by a perspective camera.
        public float FieldOfView
        {
            get => ComponentBindings.CameraGetFieldOfView(InternalId);
            set => ComponentBindings.CameraSetFieldOfView(InternalId, value);
        }

        // Half the vertical extent the view covers, in world units. Only used by an orthographic
        // camera.
        public float OrthographicSize
        {
            get => ComponentBindings.CameraGetOrthographicSize(InternalId);
            set => ComponentBindings.CameraSetOrthographicSize(InternalId, value);
        }

        public Vector4 ClearColor
        {
            get
            {
                Vector4 color;

                ComponentBindings.CameraGetClearColor(InternalId, &color);

                return color;
            }
            set => ComponentBindings.CameraSetClearColor(InternalId, &value);
        }

        // Where a world position lands on screen. X and Y are pixels within the camera's viewport;
        // Z is the distance in front of the camera, so a negative Z means the point is behind it
        // and the X/Y are meaningless.
        public Vector3 WorldToScreenPoint(Vector3 position)
        {
            Vector3 screenPoint;

            ComponentBindings.CameraWorldToScreenPoint(InternalId, &position, &screenPoint);

            return screenPoint;
        }
    }
}

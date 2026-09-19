using System.Runtime.CompilerServices;
using Pine.Math;

namespace Pine.World.Components
{
    public enum CameraType
    {
        Perspective,
        Orthographic
    }

    public class Camera : Component
    {
        public CameraType CameraType
        {
            get => (CameraType)PineGetCameraType(InternalId);
            set => PineSetCameraType(InternalId, (int)value);
        }

        public float NearPlane
        {
            get => PineGetNearPlane(InternalId);
            set => PineSetNearPlane(InternalId, value);
        }

        public float FarPlane
        {
            get => PineGetFarPlane(InternalId);
            set => PineSetFarPlane(InternalId, value);
        }

        // Vertical field of view in degrees. Only used by a perspective camera.
        public float FieldOfView
        {
            get => PineGetFieldOfView(InternalId);
            set => PineSetFieldOfView(InternalId, value);
        }

        // Half the vertical extent the view covers, in world units. Only used by an orthographic
        // camera.
        public float OrthographicSize
        {
            get => PineGetOrthographicSize(InternalId);
            set => PineSetOrthographicSize(InternalId, value);
        }

        public Vector4 ClearColor
        {
            get
            {
                PineGetClearColor(InternalId, out var color);
                return color;
            }
            set => PineSetClearColor(InternalId, ref value);
        }

        // Where a world position lands on screen. X and Y are pixels within the camera's viewport;
        // Z is the distance in front of the camera, so a negative Z means the point is behind it
        // and the X/Y are meaningless.
        public Vector3 WorldToScreenPoint(Vector3 position)
        {
            PineWorldToScreenPoint(InternalId, ref position, out var screenPoint);
            return screenPoint;
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int PineGetCameraType(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetCameraType(uint id, int type);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float PineGetNearPlane(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetNearPlane(uint id, float value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float PineGetFarPlane(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetFarPlane(uint id, float value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float PineGetFieldOfView(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetFieldOfView(uint id, float value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float PineGetOrthographicSize(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetOrthographicSize(uint id, float value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineGetClearColor(uint id, out Vector4 color);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetClearColor(uint id, ref Vector4 color);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineWorldToScreenPoint(uint id, ref Vector3 position, out Vector3 screenPoint);
    }
}

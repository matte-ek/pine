using System.Runtime.CompilerServices;
using Pine.Math;

namespace Pine.World.Components
{
    public enum LightType
    {
        Directional,
        PointLight,
        SpotLight
    }

    public class Light : Component
    {
        public LightType LightType
        {
            get => (LightType)PineGetLightType(InternalId);
            set => PineSetLightType(InternalId, (int)value);
        }

        // Linear colour, multiplied by Intensity. Values above 1 push the lit result past white in
        // the HDR buffer, which is what lets a light bloom instead of clamping.
        public Vector3 LightColor
        {
            get
            {
                PineGetLightColor(InternalId, out var color);
                return color;
            }
            set => PineSetLightColor(InternalId, ref value);
        }

        public float LightIntensity
        {
            get => PineGetLightIntensity(InternalId);
            set => PineSetLightIntensity(InternalId, value);
        }

        // How far the light reaches, in world units. Not only a performance knob - the falloff is
        // windowed to reach zero here, and the shadow far plane is placed to match.
        public float Range
        {
            get => PineGetRange(InternalId);
            set => PineSetRange(InternalId, value);
        }

        // Whether this light is a candidate for casting shadows, not a promise that it will: the
        // shadow budget picks winners among the candidates.
        public bool CastShadows
        {
            get => PineGetCastShadows(InternalId);
            set => PineSetCastShadows(InternalId, value);
        }

        // Spotlight cone half-angles in degrees. Inner is where the falloff starts, outer where it
        // reaches zero; the engine keeps inner <= outer, so the cone can never invert.
        public float SpotlightOuterAngle
        {
            get => PineGetSpotlightOuterAngle(InternalId);
            set => PineSetSpotlightOuterAngle(InternalId, value);
        }

        public float SpotlightInnerAngle
        {
            get => PineGetSpotlightInnerAngle(InternalId);
            set => PineSetSpotlightInnerAngle(InternalId, value);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int PineGetLightType(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetLightType(uint id, int type);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineGetLightColor(uint id, out Vector3 color);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetLightColor(uint id, ref Vector3 color);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float PineGetLightIntensity(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetLightIntensity(uint id, float intensity);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float PineGetRange(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetRange(uint id, float range);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool PineGetCastShadows(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetCastShadows(uint id, bool castShadows);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float PineGetSpotlightOuterAngle(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetSpotlightOuterAngle(uint id, float degrees);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float PineGetSpotlightInnerAngle(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void PineSetSpotlightInnerAngle(uint id, float degrees);
    }
}

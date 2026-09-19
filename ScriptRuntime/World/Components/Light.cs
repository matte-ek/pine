using Pine.Core.Bindings;
using Pine.Math;

namespace Pine.World.Components
{
    public enum LightType
    {
        Directional,
        PointLight,
        SpotLight
    }

    [ComponentType(ComponentType.Light)]
    public unsafe class Light : Component
    {
        public LightType LightType
        {
            get => (LightType)ComponentBindings.LightGetLightType(InternalId);
            set => ComponentBindings.LightSetLightType(InternalId, (int)value);
        }

        // Linear colour, multiplied by Intensity. Values above 1 push the lit result past white in
        // the HDR buffer, which is what lets a light bloom instead of clamping.
        public Vector3 LightColor
        {
            get
            {
                Vector3 color;

                ComponentBindings.LightGetLightColor(InternalId, &color);

                return color;
            }
            set => ComponentBindings.LightSetLightColor(InternalId, &value);
        }

        public float LightIntensity
        {
            get => ComponentBindings.LightGetLightIntensity(InternalId);
            set => ComponentBindings.LightSetLightIntensity(InternalId, value);
        }

        // How far the light reaches, in world units. Not only a performance knob - the falloff is
        // windowed to reach zero here, and the shadow far plane is placed to match.
        public float Range
        {
            get => ComponentBindings.LightGetRange(InternalId);
            set => ComponentBindings.LightSetRange(InternalId, value);
        }

        // Whether this light is a candidate for casting shadows, not a promise that it will: the
        // shadow budget picks winners among the candidates.
        public bool CastShadows
        {
            get => ComponentBindings.LightGetCastShadows(InternalId) != 0;
            set => ComponentBindings.LightSetCastShadows(InternalId, value ? (byte)1 : (byte)0);
        }

        // Spotlight cone half-angles in degrees. Inner is where the falloff starts, outer where it
        // reaches zero; the engine keeps inner <= outer, so the cone can never invert.
        public float SpotlightOuterAngle
        {
            get => ComponentBindings.LightGetSpotlightOuterAngle(InternalId);
            set => ComponentBindings.LightSetSpotlightOuterAngle(InternalId, value);
        }

        public float SpotlightInnerAngle
        {
            get => ComponentBindings.LightGetSpotlightInnerAngle(InternalId);
            set => ComponentBindings.LightSetSpotlightInnerAngle(InternalId, value);
        }
    }
}

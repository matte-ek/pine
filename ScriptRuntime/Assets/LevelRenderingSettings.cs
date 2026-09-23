using Pine.Core.Bindings;
using Pine.Math;

namespace Pine.Assets
{
    // The rendering half of a level's settings, reached through Level.Rendering. These are the
    // level asset's own values, and the renderer reads them every frame, so a change shows on the
    // next one. The editor puts them back when play mode stops; a running game keeps a change for
    // the rest of the session, including when the same level is loaded again.
    //
    // Setters clamp to the editor's minimums: nothing below zero, and a fog distance of at least 0.01.
    public unsafe class LevelRenderingSettings
    {
        private readonly Level _level;

        internal LevelRenderingSettings(Level level)
        {
            _level = level;
        }

        // Colours are sRGB, as picked in the editor.
        public Vector3 AmbientColor
        {
            get
            {
                Vector3 color;

                AssetBindings.LevelGetAmbientColor(_level.Id, &color);

                return color;
            }
            set => AssetBindings.LevelSetAmbientColor(_level.Id, &value);
        }

        public Vector4 FogColor
        {
            get
            {
                Vector4 color;

                AssetBindings.LevelGetFogColor(_level.Id, &color);

                return color;
            }
            set => AssetBindings.LevelSetFogColor(_level.Id, &value);
        }

        // Fog is linear: it starts at the camera and reaches FogIntensity at FogDistance world
        // units. An intensity of 0 turns it off. The skybox is not fogged.
        public float FogDistance
        {
            get => AssetBindings.LevelGetFogDistance(_level.Id);
            set => AssetBindings.LevelSetFogDistance(_level.Id, value);
        }

        public float FogIntensity
        {
            get => AssetBindings.LevelGetFogIntensity(_level.Id);
            set => AssetBindings.LevelSetFogIntensity(_level.Id, value);
        }

        // HDR multiplier applied before tone mapping. 1 is neutral.
        public float Exposure
        {
            get => AssetBindings.LevelGetExposure(_level.Id);
            set => AssetBindings.LevelSetExposure(_level.Id, value);
        }

        // Brightness above the threshold glows; the intensity scales that glow, and 0 turns it off.
        public float BloomThreshold
        {
            get => AssetBindings.LevelGetBloomThreshold(_level.Id);
            set => AssetBindings.LevelSetBloomThreshold(_level.Id, value);
        }

        public float BloomIntensity
        {
            get => AssetBindings.LevelGetBloomIntensity(_level.Id);
            set => AssetBindings.LevelSetBloomIntensity(_level.Id, value);
        }

        public float GrainStrength
        {
            get => AssetBindings.LevelGetGrainStrength(_level.Id);
            set => AssetBindings.LevelSetGrainStrength(_level.Id, value);
        }

        public float VignetteStrength
        {
            get => AssetBindings.LevelGetVignetteStrength(_level.Id);
            set => AssetBindings.LevelSetVignetteStrength(_level.Id, value);
        }
    }
}

using Pine.Core.Bindings;
using Pine.Math;

namespace Pine.Assets
{
    // The rendering half of a level's settings, reached through Level.Rendering. These are the
    // level asset's own values, and the renderer reads them every frame, so a change shows on the
    // next one. The editor puts them back when play mode stops; a running game keeps a change for
    // the rest of the session, including when the same level is loaded again.
    //
    // Setters clamp to the editor's minimums: nothing below zero, except FogHeight, which is a position.
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

        // Exponential height fog, over the skybox as well as geometry. FogDensity is how much fog
        // there is per world unit at FogHeight, and 0 turns it off. Looking level from FogHeight,
        // fog hides 95% of what lies 3 / FogDensity units away.
        public float FogDensity
        {
            get => AssetBindings.LevelGetFogDensity(_level.Id);
            set => AssetBindings.LevelSetFogDensity(_level.Id, value);
        }

        // The world height at which the fog is FogDensity thick.
        public float FogHeight
        {
            get => AssetBindings.LevelGetFogHeight(_level.Id);
            set => AssetBindings.LevelSetFogHeight(_level.Id, value);
        }

        // Above FogHeight, the fog thins by a factor of e every 1 / FogHeightFalloff units, so the
        // sky overhead stays clear while the horizon fades out. 0 is the same fog at every height.
        public float FogHeightFalloff
        {
            get => AssetBindings.LevelGetFogHeightFalloff(_level.Id);
            set => AssetBindings.LevelSetFogHeightFalloff(_level.Id, value);
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

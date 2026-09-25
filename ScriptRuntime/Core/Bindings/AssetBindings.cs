using System;
using Pine.Math;

namespace Pine.Core.Bindings
{
    // The engine functions behind Pine.Assets.
    internal static unsafe class AssetBindings
    {
        public static delegate* unmanaged<UId, IntPtr> GetFileName;
        public static delegate* unmanaged<UId, IntPtr> GetPath;
        public static delegate* unmanaged<byte*, ulong> GetByPath;
        public static delegate* unmanaged<UId, ulong> GetById;

        public static delegate* unmanaged<UId, float> AudioGetDuration;

        public static delegate* unmanaged<UId, byte> BlueprintGetHasEntity;
        public static delegate* unmanaged<UId, uint, void> BlueprintCreateFromEntity;
        public static delegate* unmanaged<UId, ulong> BlueprintSpawnEntity;

        public static delegate* unmanaged<UId, void> LevelCreateFromWorld;
        public static delegate* unmanaged<UId, void> LevelLoad;
        public static delegate* unmanaged<ulong> LevelGetActive;

        public static delegate* unmanaged<UId, Vector3*, void> LevelGetAmbientColor;
        public static delegate* unmanaged<UId, Vector3*, void> LevelSetAmbientColor;
        public static delegate* unmanaged<UId, Vector4*, void> LevelGetFogColor;
        public static delegate* unmanaged<UId, Vector4*, void> LevelSetFogColor;
        public static delegate* unmanaged<UId, float> LevelGetFogDensity;
        public static delegate* unmanaged<UId, float, void> LevelSetFogDensity;
        public static delegate* unmanaged<UId, float> LevelGetFogHeight;
        public static delegate* unmanaged<UId, float, void> LevelSetFogHeight;
        public static delegate* unmanaged<UId, float> LevelGetFogHeightFalloff;
        public static delegate* unmanaged<UId, float, void> LevelSetFogHeightFalloff;
        public static delegate* unmanaged<UId, float> LevelGetExposure;
        public static delegate* unmanaged<UId, float, void> LevelSetExposure;
        public static delegate* unmanaged<UId, float> LevelGetBloomThreshold;
        public static delegate* unmanaged<UId, float, void> LevelSetBloomThreshold;
        public static delegate* unmanaged<UId, float> LevelGetBloomIntensity;
        public static delegate* unmanaged<UId, float, void> LevelSetBloomIntensity;
        public static delegate* unmanaged<UId, float> LevelGetGrainStrength;
        public static delegate* unmanaged<UId, float, void> LevelSetGrainStrength;
        public static delegate* unmanaged<UId, float> LevelGetVignetteStrength;
        public static delegate* unmanaged<UId, float, void> LevelSetVignetteStrength;

        public static void Bind()
        {
            GetFileName = (delegate* unmanaged<UId, IntPtr>)Interop.Resolve("Pine.Assets.Asset::GetFileName");
            GetPath = (delegate* unmanaged<UId, IntPtr>)Interop.Resolve("Pine.Assets.Asset::GetPath");
            GetByPath = (delegate* unmanaged<byte*, ulong>)Interop.Resolve("Pine.Assets.AssetManager::GetByPath");
            GetById = (delegate* unmanaged<UId, ulong>)Interop.Resolve("Pine.Assets.AssetManager::GetById");

            AudioGetDuration = (delegate* unmanaged<UId, float>)Interop.Resolve("Pine.Assets.Audio::GetDuration");

            BlueprintGetHasEntity = (delegate* unmanaged<UId, byte>)Interop.Resolve("Pine.Assets.Blueprint::GetHasEntity");
            BlueprintCreateFromEntity = (delegate* unmanaged<UId, uint, void>)Interop.Resolve("Pine.Assets.Blueprint::CreateFromEntity");
            BlueprintSpawnEntity = (delegate* unmanaged<UId, ulong>)Interop.Resolve("Pine.Assets.Blueprint::SpawnEntity");

            LevelCreateFromWorld = (delegate* unmanaged<UId, void>)Interop.Resolve("Pine.Assets.Level::CreateFromWorld");
            LevelLoad = (delegate* unmanaged<UId, void>)Interop.Resolve("Pine.Assets.Level::Load");
            LevelGetActive = (delegate* unmanaged<ulong>)Interop.Resolve("Pine.Assets.Level::GetActive");

            LevelGetAmbientColor = (delegate* unmanaged<UId, Vector3*, void>)Interop.Resolve("Pine.Assets.LevelRenderingSettings::GetAmbientColor");
            LevelSetAmbientColor = (delegate* unmanaged<UId, Vector3*, void>)Interop.Resolve("Pine.Assets.LevelRenderingSettings::SetAmbientColor");
            LevelGetFogColor = (delegate* unmanaged<UId, Vector4*, void>)Interop.Resolve("Pine.Assets.LevelRenderingSettings::GetFogColor");
            LevelSetFogColor = (delegate* unmanaged<UId, Vector4*, void>)Interop.Resolve("Pine.Assets.LevelRenderingSettings::SetFogColor");
            LevelGetFogDensity = (delegate* unmanaged<UId, float>)Interop.Resolve("Pine.Assets.LevelRenderingSettings::GetFogDensity");
            LevelSetFogDensity = (delegate* unmanaged<UId, float, void>)Interop.Resolve("Pine.Assets.LevelRenderingSettings::SetFogDensity");
            LevelGetFogHeight = (delegate* unmanaged<UId, float>)Interop.Resolve("Pine.Assets.LevelRenderingSettings::GetFogHeight");
            LevelSetFogHeight = (delegate* unmanaged<UId, float, void>)Interop.Resolve("Pine.Assets.LevelRenderingSettings::SetFogHeight");
            LevelGetFogHeightFalloff = (delegate* unmanaged<UId, float>)Interop.Resolve("Pine.Assets.LevelRenderingSettings::GetFogHeightFalloff");
            LevelSetFogHeightFalloff = (delegate* unmanaged<UId, float, void>)Interop.Resolve("Pine.Assets.LevelRenderingSettings::SetFogHeightFalloff");
            LevelGetExposure = (delegate* unmanaged<UId, float>)Interop.Resolve("Pine.Assets.LevelRenderingSettings::GetExposure");
            LevelSetExposure = (delegate* unmanaged<UId, float, void>)Interop.Resolve("Pine.Assets.LevelRenderingSettings::SetExposure");
            LevelGetBloomThreshold = (delegate* unmanaged<UId, float>)Interop.Resolve("Pine.Assets.LevelRenderingSettings::GetBloomThreshold");
            LevelSetBloomThreshold = (delegate* unmanaged<UId, float, void>)Interop.Resolve("Pine.Assets.LevelRenderingSettings::SetBloomThreshold");
            LevelGetBloomIntensity = (delegate* unmanaged<UId, float>)Interop.Resolve("Pine.Assets.LevelRenderingSettings::GetBloomIntensity");
            LevelSetBloomIntensity = (delegate* unmanaged<UId, float, void>)Interop.Resolve("Pine.Assets.LevelRenderingSettings::SetBloomIntensity");
            LevelGetGrainStrength = (delegate* unmanaged<UId, float>)Interop.Resolve("Pine.Assets.LevelRenderingSettings::GetGrainStrength");
            LevelSetGrainStrength = (delegate* unmanaged<UId, float, void>)Interop.Resolve("Pine.Assets.LevelRenderingSettings::SetGrainStrength");
            LevelGetVignetteStrength = (delegate* unmanaged<UId, float>)Interop.Resolve("Pine.Assets.LevelRenderingSettings::GetVignetteStrength");
            LevelSetVignetteStrength = (delegate* unmanaged<UId, float, void>)Interop.Resolve("Pine.Assets.LevelRenderingSettings::SetVignetteStrength");
        }
    }
}

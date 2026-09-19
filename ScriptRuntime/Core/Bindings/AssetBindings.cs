using System;

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
        }
    }
}

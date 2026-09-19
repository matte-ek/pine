using Pine.Core;
using Pine.Core.Bindings;

namespace Pine.Assets
{
    public enum AssetType
    {
        Invalid,
        Blueprint,
        Level,
        Material,
        Mesh,
        Model,
        Shader,
        Texture2D,
        Texture3D,
        Font,
        Tileset,
        Tilemap,
        Audio,
        CSharpScript,
        Terrain,
    }

    public unsafe class Asset
    {
        public readonly AssetType Type = AssetType.Invalid;

        // Assets are identified by their UId (mirror of the native Pine::UId). There is no
        // array-slot id for assets, unlike entities/components.
        public readonly UId Id;

        public string FileName => Interop.StringFrom(AssetBindings.GetFileName(Id));
        public string Path => Interop.StringFrom(AssetBindings.GetPath(Id));
    }
}

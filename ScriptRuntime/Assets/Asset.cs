using System.Runtime.CompilerServices;
using Pine.Core;

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
        CSharpScript,
    }
    
    public class Asset
    {
        public readonly AssetType Type = AssetType.Invalid;

        // Assets are identified by their UId (mirror of the native Pine::UId). There is no
        // array-slot id for assets, unlike entities/components.
        public readonly UId Id;

        public string FileName => GetFileName(Id);
        public string Path => GetPath(Id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern string GetFileName(UId id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern string GetPath(UId id);
    }
}
using System.Runtime.CompilerServices;

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
        protected uint _internalId = 0;
        
        public string FileName => GetFileName(_internalId);
        public string Path => GetFileName(_internalId);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern string GetFileName(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern string GetPath(uint id);
    }
}
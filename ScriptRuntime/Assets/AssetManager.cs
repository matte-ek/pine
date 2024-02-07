using System.Runtime.CompilerServices;

namespace Pine.Assets
{
    public class AssetManager
    {
        public static T Get<T>(string path) where T : Asset
        {
            return (T)GetByPath(path);
        }
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Asset GetByPath(string path);
    }
}
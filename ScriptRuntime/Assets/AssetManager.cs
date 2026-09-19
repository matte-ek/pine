using Pine.Core;
using Pine.Core.Bindings;

namespace Pine.Assets
{
    public unsafe class AssetManager
    {
        public static T Get<T>(string path) where T : Asset
        {
            using var text = new Interop.Utf8Scope(path);

            return Interop.ObjectFrom<T>(AssetBindings.GetByPath(text.Pointer));
        }

        // Not public: an authored asset reference is stored as its UId, so the field registry needs
        // this to put a script's asset field back, but a game reaches its assets by path.
        internal static Asset GetByUId(UId id)
        {
            return Interop.ObjectFrom<Asset>(AssetBindings.GetById(id));
        }
    }
}

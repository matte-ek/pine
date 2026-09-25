using Pine.Assets;
using Pine.Core;
using Pine.Core.Bindings;

namespace Pine.World.Components
{
    [ComponentType(ComponentType.ModelRenderer)]
    public unsafe class ModelRenderer : Component
    {
        public Model Model
        {
            get => Interop.ObjectFrom<Model>(ComponentBindings.GetModel(InternalId));
            set => ComponentBindings.SetModel(InternalId, value.Id);
        }

        // Whether this object is drawn into shadow maps. Independent of ReceiveShadows.
        public bool CastShadows
        {
            get => ComponentBindings.ModelRendererGetCastShadows(InternalId) != 0;
            set => ComponentBindings.ModelRendererSetCastShadows(InternalId, value ? (byte)1 : (byte)0);
        }

        // Whether shadows darken this object.
        public bool ReceiveShadows
        {
            get => ComponentBindings.ModelRendererGetReceiveShadows(InternalId) != 0;
            set => ComponentBindings.ModelRendererSetReceiveShadows(InternalId, value ? (byte)1 : (byte)0);
        }
    }
}

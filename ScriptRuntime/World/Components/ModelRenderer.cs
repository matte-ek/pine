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
    }
}

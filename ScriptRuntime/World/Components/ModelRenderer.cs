using System;
using System.Runtime.CompilerServices;
using Pine.Assets;

namespace Pine.World.Components
{
    public class ModelRenderer : Component
    {
        public Model Model
        {
            get => (Model)GetModel(_internalId);
            set => SetModel(_internalId, value._internalId);
        }
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Asset GetModel(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetModel(uint id, uint assetId);   
    }
}
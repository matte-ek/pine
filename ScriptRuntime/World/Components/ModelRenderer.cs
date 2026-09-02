using System;
using System.Runtime.CompilerServices;
using Pine.Assets;
using Pine.Core;

namespace Pine.World.Components
{
    public class ModelRenderer : Component
    {
        public Model Model
        {
            get => (Model)GetModel(InternalId);
            set => SetModel(InternalId, value.Id);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Asset GetModel(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetModel(uint id, UId assetId);
    }
}
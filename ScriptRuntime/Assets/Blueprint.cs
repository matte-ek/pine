using System.Runtime.CompilerServices;
using Pine.World;

namespace Pine.Assets
{
    public class Blueprint : Asset
    {
        public bool HasEntity => GetHasEntity(_internalId);
        public void CreateFromEntity(Entity entity) => CreateFromEntity(_internalId, entity._internalId);
        public Entity SpawnEntity() => SpawnEntity(_internalId);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool GetHasEntity(uint id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void CreateFromEntity(uint id, uint entityId);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Entity SpawnEntity(uint id);
    }
}
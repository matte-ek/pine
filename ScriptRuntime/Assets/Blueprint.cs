using System.Runtime.CompilerServices;
using Pine.Core;
using Pine.World;

namespace Pine.Assets
{
    public class Blueprint : Asset
    {
        public bool HasEntity => GetHasEntity(Id);
        public void CreateFromEntity(Entity entity) => CreateFromEntity(Id, entity.InternalId);
        public Entity SpawnEntity() => SpawnEntity(Id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool GetHasEntity(UId id);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void CreateFromEntity(UId id, uint entityId);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Entity SpawnEntity(UId id);
    }
}

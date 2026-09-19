using Pine.Core;
using Pine.Core.Bindings;
using Pine.World;

namespace Pine.Assets
{
    public unsafe class Blueprint : Asset
    {
        public bool HasEntity => AssetBindings.BlueprintGetHasEntity(Id) != 0;

        public void CreateFromEntity(Entity entity)
            => AssetBindings.BlueprintCreateFromEntity(Id, entity.InternalId);

        public Entity SpawnEntity()
            => Interop.ObjectFrom<Entity>(AssetBindings.BlueprintSpawnEntity(Id));
    }
}

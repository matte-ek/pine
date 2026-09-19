using Pine.Core;
using Pine.Core.Bindings;

namespace Pine.World
{
    public static unsafe class EntityList
    {
        public static Entity Create() => Entity.Create();
        public static Entity Create(string name) => Entity.Create(name);

        public static Entity Find(string name)
        {
            using var text = new Interop.Utf8Scope(name);

            return Interop.ObjectFrom<Entity>(EntityBindings.FindByName(text.Pointer));
        }

        public static Entity[] Find(ulong tag) => FindByTag(tag);

        public static Entity[] GetAll()
        {
            var entities = new Entity[EntityBindings.GetCount()];

            for (var index = 0; index < entities.Length; index++)
            {
                entities[index] = Interop.ObjectFrom<Entity>(EntityBindings.GetAt(index));
            }

            return entities;
        }

        private static Entity[] FindByTag(ulong tag)
        {
            var entities = new Entity[EntityBindings.FindByTagCount(tag)];

            for (var index = 0; index < entities.Length; index++)
            {
                entities[index] = Interop.ObjectFrom<Entity>(EntityBindings.FindByTagAt(tag, index));
            }

            return entities;
        }
    }
}

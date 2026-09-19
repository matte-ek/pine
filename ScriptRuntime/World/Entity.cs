using System.Collections.Generic;
using Pine.Core;
using Pine.Core.Bindings;
using Pine.World.Components;

namespace Pine.World
{
    public unsafe class Entity
    {
        public readonly UId Id;
        public bool IsValid => _isValid == 1;

        public string Name
        {
            get => Interop.StringFrom(EntityBindings.GetName(InternalId));
            set
            {
                using var name = new Interop.Utf8Scope(value);

                EntityBindings.SetName(InternalId, name.Pointer);
            }
        }

        public bool Active
        {
            get => EntityBindings.GetActive(InternalId) != 0;
            set => EntityBindings.SetActive(InternalId, value ? (byte)1 : (byte)0);
        }

        public bool Static
        {
            get => EntityBindings.GetStatic(InternalId) != 0;
            set => EntityBindings.SetStatic(InternalId, value ? (byte)1 : (byte)0);
        }

        public ulong Tags
        {
            get => EntityBindings.GetTags(InternalId);
            set => EntityBindings.SetTags(InternalId, value);
        }

        public IEnumerable<Entity> Children
        {
            get
            {
                var id = InternalId;
                var children = new Entity[EntityBindings.GetChildCount(id)];

                for (var index = 0; index < children.Length; index++)
                {
                    children[index] = Interop.ObjectFrom<Entity>(EntityBindings.GetChild(id, index));
                }

                return children;
            }
        }

        public void Destroy() => EntityBindings.DestroyEntity(_internalId);

        public Transform Transform => Interop.ObjectFrom<Transform>(EntityBindings.GetTransform(InternalId));

        public bool HasComponent<T>() where T: Component
            => EntityBindings.HasComponent(InternalId, ComponentTypes.Of<T>()) != 0;

        public T AddComponent<T>() where T : Component
            => Interop.ObjectFrom<T>(EntityBindings.AddComponent(InternalId, ComponentTypes.Of<T>()));

        public T GetComponent<T>() where T : Component
            => Interop.ObjectFrom<T>(EntityBindings.GetComponent(InternalId, ComponentTypes.Of<T>()));

        public T[] GetComponents<T>() where T : Component
        {
            var id = InternalId;
            var type = ComponentTypes.Of<T>();
            var components = new T[EntityBindings.GetComponentCount(id, type)];

            for (var index = 0; index < components.Length; index++)
            {
                components[index] = Interop.ObjectFrom<T>(EntityBindings.GetComponentAt(id, type, index));
            }

            return components;
        }

        public T GetScript<T>() where T : Script
        {
            foreach (var component in GetComponents<Script>())
            {
                if (component is T scriptInstance)
                {
                    return scriptInstance;
                }
            }

            return null;
        }

        internal uint InternalId
        {
            get
            {
                if (_isValid == 0)
                {
                    Log.Error("Attempt to access invalid entity");
                    return uint.MaxValue;
                }

                return _internalId;
            }
        }

        protected Entity()
        {
        }

        private uint _internalId = 0;
        private int _isValid = 1;

        public static Entity Create() => Create("");

        public static Entity Create(string name)
        {
            using var text = new Interop.Utf8Scope(name);

            return Interop.ObjectFrom<Entity>(EntityBindings.CreateEntity(text.Pointer));
        }
    }
}

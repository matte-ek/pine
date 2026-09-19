using System;
using Pine.Core;

namespace Pine.World
{
    // Names the engine ComponentType that a managed component class stands for.
    //
    // The engine identifies a component by that value, so Entity's generic methods resolve T to it
    // and hand the value across rather than a System.Type. The mapping lives on the class so that
    // adding a component under World/Components/ and naming its engine type happen in one place.
    [AttributeUsage(AttributeTargets.Class, Inherited = true)]
    internal sealed class ComponentTypeAttribute : Attribute
    {
        public ComponentTypeAttribute(ComponentType type)
        {
            Type = type;
        }

        public ComponentType Type { get; }
    }

    internal static class ComponentTypes
    {
        // What a binding is handed for a class that names no component type. Every binding that
        // takes a component type treats this as "no such component", rather than acting on the
        // wrong one.
        public const int Unknown = -1;

        public static int Of<T>() where T : Component
        {
            return Resolved<T>.Value;
        }

        // Resolved once per T, because the answer cannot change while the assembly is loaded.
        private static class Resolved<T> where T : Component
        {
            public static readonly int Value = Resolve();

            private static int Resolve()
            {
                // Inherited attributes count, so a game's own `class Player : Script` resolves to
                // Script - the component it actually is.
                var attribute = (ComponentTypeAttribute)Attribute.GetCustomAttribute(
                    typeof(T), typeof(ComponentTypeAttribute), true);

                if (attribute == null)
                {
                    Log.Error($"{typeof(T).FullName} does not name an engine component type.");

                    return Unknown;
                }

                return (int)attribute.Type;
            }
        }
    }
}

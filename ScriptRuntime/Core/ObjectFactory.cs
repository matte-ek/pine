using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;
using Pine.Assets;
using Pine.World;

namespace Pine.Core
{
    // The managed mirrors of engine objects.
    //
    // Every Entity, Component and Asset the engine hands to C# has an object here standing for it,
    // anchored by a GC handle whose value the engine holds as its Pine::Script::ObjectHandle.
    // Making those objects lives on this side because all of it - finding the class, creating an
    // instance, writing the identity the engine addresses it by - is reflection, which the engine
    // would otherwise have to do through whichever runtime is hosting us.
    //
    // Every method the engine calls is an entry point across a native boundary, so none of them may
    // let an exception escape: each one catches, logs and answers with a failure value, which for
    // the four Create entry points is a handle to nothing - the engine's "no object".
    internal static class ObjectFactory
    {
        [UnmanagedCallersOnly]
        public static ulong CreateEntity(UId id, uint internalId)
        {
            try
            {
                var entity = Activator.CreateInstance(typeof(Entity), true);

                EntityIdField.SetValue(entity, id);
                EntityInternalIdField.SetValue(entity, internalId);

                return Anchor(entity);
            }
            catch (Exception exception)
            {
                Log.Error("Failed to create the managed mirror of an entity: " + exception);

                return 0;
            }
        }

        [UnmanagedCallersOnly]
        public static ulong CreateComponent(int componentType, uint internalId, ulong parentHandle)
        {
            try
            {
                var componentClass = ComponentClassOf(componentType);

                if (componentClass == null)
                {
                    return 0;
                }

                var component = Activator.CreateInstance(componentClass, true);

                WriteComponentIdentity(component, componentType, internalId, parentHandle);

                return Anchor(component);
            }
            catch (Exception exception)
            {
                Log.Error("Failed to create the managed mirror of a component: " + exception);

                return 0;
            }
        }

        // The instance of a game's own script class that backs one ScriptComponent. Its class comes
        // from the engine rather than from a lookup here, because it lives in the game assembly -
        // which is where the engine resolved it from in the first place.
        [UnmanagedCallersOnly]
        public static ulong CreateScriptObject(ulong typeHandle, int componentType, uint internalId, ulong parentHandle)
        {
            try
            {
                var scriptClass = Interop.ObjectFrom<Type>(typeHandle);

                if (scriptClass == null)
                {
                    Log.Error("Script object creation was handed something that is not a type.");

                    return 0;
                }

                var script = Activator.CreateInstance(scriptClass, true);

                WriteComponentIdentity(script, componentType, internalId, parentHandle);

                return Anchor(script);
            }
            catch (Exception exception)
            {
                Log.Error("Failed to create the managed instance of a script: " + exception);

                return 0;
            }
        }

        [UnmanagedCallersOnly]
        public static ulong CreateAsset(int assetType, UId id)
        {
            try
            {
                var assetClass = AssetClassOf(assetType);

                if (assetClass == null)
                {
                    return 0;
                }

                var asset = Activator.CreateInstance(assetClass, true);

                // An asset has no array-slot id, unlike an entity or a component: its UId is the
                // whole of its identity, and every asset binding is addressed by it.
                AssetIdField.SetValue(asset, id);
                AssetTypeField.SetValue(asset, Enum.ToObject(AssetTypeField.FieldType, assetType));

                return Anchor(asset);
            }
            catch (Exception exception)
            {
                Log.Error("Failed to create the managed mirror of an asset: " + exception);

                return 0;
            }
        }

        [UnmanagedCallersOnly]
        public static void DisposeEntity(ulong handle)
        {
            Invalidate(handle, EntityValidField, 0);

            Release(handle);
        }

        [UnmanagedCallersOnly]
        public static void DisposeComponent(ulong handle)
        {
            Invalidate(handle, ComponentValidField, false);

            Release(handle);
        }

        // For the mirrors that have no valid flag to clear - today, the script instance behind a
        // ScriptComponent, which the engine drops whole rather than invalidating.
        [UnmanagedCallersOnly]
        public static void DisposeObject(ulong handle)
        {
            Release(handle);
        }

        // -------------------------------------------------------------------------------------

        private static readonly FieldInfo EntityIdField = FieldOf(typeof(Entity), "Id");
        private static readonly FieldInfo EntityInternalIdField = FieldOf(typeof(Entity), "_internalId");
        private static readonly FieldInfo EntityValidField = FieldOf(typeof(Entity), "_isValid");

        // Every component class inherits these from Component, script classes included, so one set
        // covers all of them and there is nothing to cache per class.
        private static readonly FieldInfo ComponentParentField = FieldOf(typeof(Component), "Parent");
        private static readonly FieldInfo ComponentTypeField = FieldOf(typeof(Component), "Type");
        private static readonly FieldInfo ComponentInternalIdField = FieldOf(typeof(Component), "_internalId");
        private static readonly FieldInfo ComponentValidField = FieldOf(typeof(Component), "_isValid");

        private static readonly FieldInfo AssetIdField = FieldOf(typeof(Asset), "Id");
        private static readonly FieldInfo AssetTypeField = FieldOf(typeof(Asset), "Type");

        // Which managed class stands for an engine type. Neither answer can change while Pine.dll
        // is loaded, and a type with no class of its own is remembered as one, so the miss is
        // reported once rather than every time the engine creates one.
        private static readonly Dictionary<int, Type> ComponentClasses = BuildComponentClasses();
        private static readonly Dictionary<int, Type> AssetClasses = new Dictionary<int, Type>();

        private static void WriteComponentIdentity(object component, int componentType, uint internalId, ulong parentHandle)
        {
            ComponentParentField.SetValue(component, Interop.ObjectFrom<Entity>(parentHandle));
            ComponentTypeField.SetValue(component, Enum.ToObject(ComponentTypeField.FieldType, componentType));
            ComponentInternalIdField.SetValue(component, internalId);
        }

        // Hold the object for the engine, and hand back the handle value it will refer to it by.
        // The handle is strong but unpinned: the engine only ever passes the value back, so there
        // is no address for the collector to keep still.
        private static ulong Anchor(object instance)
        {
            return (ulong)GCHandle.ToIntPtr(GCHandle.Alloc(instance)).ToInt64();
        }

        private static void Release(ulong handle)
        {
            try
            {
                if (handle != 0)
                {
                    GCHandle.FromIntPtr(new IntPtr((long)handle)).Free();
                }
            }
            catch (Exception exception)
            {
                Log.Error("Failed to release the managed mirror of an engine object: " + exception);
            }
        }

        // Tell the C# side that the engine object behind a mirror is gone. A script may still be
        // holding the mirror, and this is what makes it say so rather than reach through it into a
        // slot that has since been reused.
        private static void Invalidate(ulong handle, FieldInfo validField, object invalid)
        {
            try
            {
                var instance = Interop.ObjectFrom<object>(handle);

                if (instance != null && validField != null)
                {
                    validField.SetValue(instance, invalid);
                }
            }
            catch (Exception exception)
            {
                Log.Error("Failed to invalidate the managed mirror of an engine object: " + exception);
            }
        }

        // Every class that names an engine component type, read off the same attribute that answers
        // the other direction - see ComponentTypes.Of<T>. One attribute for both directions is what
        // stops them drifting apart, and it is declared rather than inherited here so that a game's
        // own script class does not register itself as the Script component.
        private static Dictionary<int, Type> BuildComponentClasses()
        {
            var classes = new Dictionary<int, Type>();

            foreach (var candidate in typeof(Component).Assembly.GetTypes())
            {
                var attribute = (ComponentTypeAttribute)Attribute.GetCustomAttribute(
                    candidate, typeof(ComponentTypeAttribute), false);

                if (attribute != null)
                {
                    classes[(int)attribute.Type] = candidate;
                }
            }

            return classes;
        }

        private static Type ComponentClassOf(int componentType)
        {
            Type found;

            if (ComponentClasses.TryGetValue(componentType, out found))
            {
                return found;
            }

            // Several of the engine's component types have no C# class at all, so this is a gap
            // rather than a fault - but a silent one would look exactly like a class that was
            // written and never given its attribute.
            Log.Warning("Pine.dll has no class for the " + Named(typeof(ComponentType), componentType)
                        + " component type, so C# will be handed nothing for those components.");

            ComponentClasses[componentType] = null;

            return null;
        }

        // The managed asset classes are named exactly as the engine names their asset types, which
        // is the same rule FieldRegistry classifies an asset field by. Nothing enforces it, so a
        // class named something else reads as a type with no class at all - said once here, rather
        // than leaving every binding for that type quietly handing back null.
        private static Type AssetClassOf(int assetType)
        {
            Type found;

            if (AssetClasses.TryGetValue(assetType, out found))
            {
                return found;
            }

            found = typeof(Asset).Assembly.GetType("Pine.Assets." + Named(typeof(AssetType), assetType));

            if (found == null)
            {
                Log.Warning("Pine.dll has no class for the " + Named(typeof(AssetType), assetType)
                            + " asset type, so C# will be handed nothing for those assets.");
            }

            AssetClasses[assetType] = found;

            return found;
        }

        private static FieldInfo FieldOf(Type type, string name)
        {
            var field = type.GetField(name,
                BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance);

            if (field == null)
            {
                Log.Error(type.FullName + " has no " + name + " field, so the engine cannot identify "
                          + "its managed mirrors. Pine.dll and the engine are out of step.");
            }

            return field;
        }

        // The enum member's name, or the bare number when the engine knows a value this build does
        // not - which is worth reading as a number rather than as nothing at all.
        private static string Named(Type enumType, int value)
        {
            return Enum.GetName(enumType, value) ?? value.ToString();
        }
    }
}

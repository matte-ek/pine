using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using Pine.Assets;
using Pine.Math;
using Pine.World;
using Pine.World.Components;

namespace Pine.Core.Reflection
{
    // Which fields of a script class the editor shows, and how its values cross to the engine.
    //
    // This is the whole of the engine's field reflection: the engine hands over a script class,
    // gets back a descriptor per field, and from then on reads and writes those fields by index.
    // Deciding what to reflect lives here rather than in the engine because the decision is made
    // from custom attributes, which only managed code can read.
    //
    // Every method the engine calls is an entry point across a native boundary, so none of them may
    // let an exception escape: each one catches, logs and answers with a failure value (-1 for a
    // count or a length, 0 for a success flag).
    internal static class FieldRegistry
    {
        // Registered classes are addressed by their position here, which is what the engine holds
        // as a class id. The list is rebuilt from nothing on every reload - see Reset.
        private static readonly List<RegisteredClass> Classes = new List<RegisteredClass>();

        // Reflect a script class and return the id the engine addresses it by, or -1.
        [UnmanagedCallersOnly]
        public static int Register(ulong typeHandle)
        {
            try
            {
                var type = Interop.ObjectFrom<Type>(typeHandle);

                if (type == null)
                {
                    Log.Error("Script field reflection was handed something that is not a type.");

                    return -1;
                }

                var registered = new RegisteredClass(type);

                Classes.Add(registered);

                return Classes.Count - 1;
            }
            catch (Exception exception)
            {
                Log.Error("Failed to reflect a script class: " + exception);

                return -1;
            }
        }

        [UnmanagedCallersOnly]
        public static int GetFieldCount(int classId)
        {
            try
            {
                var registered = ClassOf(classId);

                return registered == null ? -1 : registered.Descriptors.Length;
            }
            catch (Exception exception)
            {
                Log.Error("Failed to count a script class' fields: " + exception);

                return -1;
            }
        }

        // Copy every descriptor into a buffer of at least GetFieldCount entries, and return how
        // many were written, or -1.
        [UnmanagedCallersOnly]
        public static int ReadDescriptors(int classId, IntPtr buffer, int capacity)
        {
            try
            {
                var registered = ClassOf(classId);

                if (registered == null)
                {
                    return -1;
                }

                var descriptors = registered.Descriptors;

                if (capacity < descriptors.Length)
                {
                    Log.Error("Script field descriptors did not fit the engine's buffer.");

                    return -1;
                }

                var stride = Marshal.SizeOf(typeof(ScriptFieldDescriptor));

                for (var index = 0; index < descriptors.Length; index++)
                {
                    Marshal.StructureToPtr(descriptors[index], buffer + index * stride, false);
                }

                return descriptors.Length;
            }
            catch (Exception exception)
            {
                Log.Error("Failed to hand over a script class' field descriptors: " + exception);

                return -1;
            }
        }

        // What one descriptor occupies, so the engine can check its own struct agrees before it
        // reads an array of them.
        [UnmanagedCallersOnly]
        public static int GetDescriptorSize()
        {
            try
            {
                return Marshal.SizeOf(typeof(ScriptFieldDescriptor));
            }
            catch (Exception exception)
            {
                Log.Error("Failed to measure a script field descriptor: " + exception);

                return -1;
            }
        }

        // How many bytes this field's current value occupies, or -1 if it cannot be read. The
        // answer varies per call for a string, which is why reading is two calls and not one.
        [UnmanagedCallersOnly]
        public static int MeasureValue(ulong objectHandle, int classId, int fieldIndex)
        {
            try
            {
                var field = FieldOf(classId, fieldIndex);
                var instance = Interop.ObjectFrom<object>(objectHandle);

                if (field == null || instance == null)
                {
                    return -1;
                }

                switch (field.Type)
                {
                    case ScriptFieldType.String:
                        return Encoding.UTF8.GetByteCount((string)field.Info.GetValue(instance) ?? string.Empty);

                    // A reference to nothing is stored as no bytes at all, so an asset field only
                    // measures a UId once it points at something.
                    case ScriptFieldType.Asset:
                        return field.Info.GetValue(instance) == null ? 0 : UIdSize;

                    default:
                        return FixedValueSize(field.Type);
                }
            }
            catch (Exception exception)
            {
                Log.Error("Failed to measure a script field: " + exception);

                return -1;
            }
        }

        // Write the field's value into buffer in the form the engine stores it, and return how many
        // bytes that took, or -1.
        [UnmanagedCallersOnly]
        public static int ReadValue(ulong objectHandle, int classId, int fieldIndex, IntPtr buffer, int capacity)
        {
            try
            {
                var field = FieldOf(classId, fieldIndex);
                var instance = Interop.ObjectFrom<object>(objectHandle);

                if (field == null || instance == null)
                {
                    return -1;
                }

                var value = field.Info.GetValue(instance);

                switch (field.Type)
                {
                    // One byte, whatever Marshal.SizeOf(typeof(bool)) says: the engine's stored
                    // layout says a bool is a byte, and saved components depend on it.
                    case ScriptFieldType.Boolean:
                        if (capacity < 1)
                        {
                            return -1;
                        }

                        Marshal.WriteByte(buffer, 0, (bool)value ? (byte)1 : (byte)0);

                        return 1;

                    case ScriptFieldType.String:
                        return WriteBytes(Encoding.UTF8.GetBytes((string)value ?? string.Empty), buffer, capacity);

                    case ScriptFieldType.Asset:
                        if (value == null)
                        {
                            return 0;
                        }

                        if (capacity < UIdSize)
                        {
                            return -1;
                        }

                        Marshal.StructureToPtr(((Asset)value).Id, buffer, false);

                        return UIdSize;

                    default:
                        var size = FixedValueSize(field.Type);

                        if (size == 0 || capacity < size)
                        {
                            return -1;
                        }

                        Marshal.StructureToPtr(value, buffer, false);

                        return size;
                }
            }
            catch (Exception exception)
            {
                Log.Error("Failed to read a script field: " + exception);

                return -1;
            }
        }

        // Set the field from the engine's stored form. Returns 1 when it wrote, 0 when it did not.
        [UnmanagedCallersOnly]
        public static int WriteValue(ulong objectHandle, int classId, int fieldIndex, IntPtr buffer, int length)
        {
            try
            {
                var field = FieldOf(classId, fieldIndex);
                var instance = Interop.ObjectFrom<object>(objectHandle);

                if (field == null || instance == null || length < 0)
                {
                    return 0;
                }

                switch (field.Type)
                {
                    case ScriptFieldType.Boolean:
                        if (length != 1)
                        {
                            return 0;
                        }

                        field.Info.SetValue(instance, Marshal.ReadByte(buffer, 0) != 0);

                        return 1;

                    case ScriptFieldType.String:
                        field.Info.SetValue(instance, Encoding.UTF8.GetString(ReadBytes(buffer, length)));

                        return 1;

                    case ScriptFieldType.Asset:
                        // Assigned even when the id resolves to nothing: the asset may since have
                        // been deleted, and leaving the C# initializer in place would be a quieter
                        // kind of wrong.
                        var asset = length == UIdSize
                            ? AssetManager.GetByUId((UId)Marshal.PtrToStructure(buffer, typeof(UId)))
                            : null;

                        field.Info.SetValue(instance, asset);

                        return 1;

                    default:
                        var size = FixedValueSize(field.Type);

                        if (size == 0 || length != size)
                        {
                            return 0;
                        }

                        field.Info.SetValue(instance, Marshal.PtrToStructure(buffer, field.Info.FieldType));

                        return 1;
                }
            }
            catch (Exception exception)
            {
                Log.Error("Failed to write a script field: " + exception);

                return 0;
            }
        }

        // Drop every registration, and with it the strings the descriptors point at. The engine
        // calls this before it reflects the script classes afresh, which it does on every reload.
        [UnmanagedCallersOnly]
        public static void Reset()
        {
            try
            {
                foreach (var registered in Classes)
                {
                    registered.Release();
                }

                Classes.Clear();
            }
            catch (Exception exception)
            {
                Log.Error("Failed to reset the script field registry: " + exception);
            }
        }

        // -------------------------------------------------------------------------------------

        private const int UIdSize = 16;

        private static RegisteredClass ClassOf(int classId)
        {
            if (classId < 0 || classId >= Classes.Count)
            {
                Log.Error("The engine asked for script class " + classId + ", which is not registered.");

                return null;
            }

            return Classes[classId];
        }

        private static ReflectedField FieldOf(int classId, int fieldIndex)
        {
            var registered = ClassOf(classId);

            if (registered == null)
            {
                return null;
            }

            if (fieldIndex < 0 || fieldIndex >= registered.Fields.Count)
            {
                Log.Error("The engine asked for field " + fieldIndex + " of a class that has "
                          + registered.Fields.Count + ".");

                return null;
            }

            return registered.Fields[fieldIndex];
        }

        // How many bytes a value of this type occupies once stored. Zero for the types whose length
        // depends on the value, and for the ones that are reflected but not stored.
        // Mirrors Pine::ScriptFieldTypeSize.
        private static int FixedValueSize(ScriptFieldType type)
        {
            switch (type)
            {
                case ScriptFieldType.Boolean:
                    return 1;
                case ScriptFieldType.Integer:
                    return sizeof(int);
                case ScriptFieldType.Float:
                    return sizeof(float);
                case ScriptFieldType.Vector2:
                    return sizeof(float) * 2;
                case ScriptFieldType.Vector3:
                    return sizeof(float) * 3;
                case ScriptFieldType.Vector4:
                    return sizeof(float) * 4;
                case ScriptFieldType.Asset:
                    return UIdSize;
                default:
                    return 0;
            }
        }

        private static int WriteBytes(byte[] bytes, IntPtr buffer, int capacity)
        {
            if (capacity < bytes.Length)
            {
                return -1;
            }

            Marshal.Copy(bytes, 0, buffer, bytes.Length);

            return bytes.Length;
        }

        private static byte[] ReadBytes(IntPtr buffer, int length)
        {
            var bytes = new byte[length];

            Marshal.Copy(buffer, bytes, 0, length);

            return bytes;
        }

        private sealed class ReflectedField
        {
            public ReflectedField(FieldInfo info, ScriptFieldType type, AssetType assetType)
            {
                Info = info;
                Type = type;
                AssetType = assetType;
            }

            public FieldInfo Info { get; }
            public ScriptFieldType Type { get; }
            public AssetType AssetType { get; }
        }

        private sealed class RegisteredClass
        {
            public RegisteredClass(Type type)
            {
                foreach (var info in DeclaredFields(type))
                {
                    if (!ShouldReflect(info))
                    {
                        continue;
                    }

                    AssetType assetType;
                    var fieldType = Classify(info.FieldType, out assetType);

                    // A type the engine can neither show nor store. The field still works in C#; it
                    // just isn't reflected, so nothing downstream has to keep checking for it.
                    if (fieldType == ScriptFieldType.Invalid || !HasExpectedLayout(info, fieldType))
                    {
                        continue;
                    }

                    Fields.Add(new ReflectedField(info, fieldType, assetType));
                }

                Descriptors = new ScriptFieldDescriptor[Fields.Count];

                for (var index = 0; index < Fields.Count; index++)
                {
                    Descriptors[index] = Describe(Fields[index]);
                }
            }

            public List<ReflectedField> Fields { get; } = new List<ReflectedField>();
            public ScriptFieldDescriptor[] Descriptors { get; }

            public void Release()
            {
                foreach (var text in _strings)
                {
                    Marshal.FreeHGlobal(text);
                }

                _strings.Clear();
            }

            private readonly List<IntPtr> _strings = new List<IntPtr>();

            private ScriptFieldDescriptor Describe(ReflectedField field)
            {
                var range = field.Info.GetCustomAttribute<RangeAttribute>();
                var header = field.Info.GetCustomAttribute<HeaderAttribute>();
                var tooltip = field.Info.GetCustomAttribute<TooltipAttribute>();

                var flags = ScriptFieldFlags.None;

                if (range != null)
                {
                    flags |= ScriptFieldFlags.HasRange;
                }

                if (field.Info.GetCustomAttribute<SpaceAttribute>() != null)
                {
                    flags |= ScriptFieldFlags.HasSpace;
                }

                return new ScriptFieldDescriptor
                {
                    Name = Hold(field.Info.Name),
                    Type = (int)field.Type,
                    AssetType = (int)field.AssetType,
                    Flags = (uint)flags,
                    RangeMin = range == null ? 0f : range.Min,
                    RangeMax = range == null ? 0f : range.Max,
                    Header = header == null ? IntPtr.Zero : Hold(header.Text),
                    Tooltip = tooltip == null ? IntPtr.Zero : Hold(tooltip.Text)
                };
            }

            // Keep a UTF-8 copy of a descriptor string alive for as long as this registration is.
            private IntPtr Hold(string text)
            {
                var allocated = Interop.AllocUtf8(text);

                _strings.Add(allocated);

                return allocated;
            }
        }

        // The class' own fields, then those of any script it derives from, stopping at the engine's
        // Script - so a script deriving from another script reflects both, and neither picks up
        // Component's Parent and Type.
        //
        // The stop matters: asking for public instance fields hands back inherited ones too, so
        // without it every script in the properties panel would grow a Parent and a Type row.
        private static IEnumerable<FieldInfo> DeclaredFields(Type type)
        {
            var hierarchy = new List<Type>();

            for (var current = type; current != null && current != typeof(Script); current = current.BaseType)
            {
                hierarchy.Add(current);
            }

            // Base first, so an inherited field keeps its place above the ones declared after it.
            hierarchy.Reverse();

            foreach (var declaring in hierarchy)
            {
                var declared = declaring.GetFields(BindingFlags.Public | BindingFlags.NonPublic
                                                   | BindingFlags.Instance | BindingFlags.DeclaredOnly);

                foreach (var info in declared)
                {
                    yield return info;
                }
            }
        }

        private static bool ShouldReflect(FieldInfo field)
        {
            // Costs two string compares, and catches a third public field being added to Component
            // above whatever the hierarchy walk stopped at.
            if (field.Name == "Parent" || field.Name == "Type")
            {
                return false;
            }

            if (field.GetCustomAttribute<HideInInspectorAttribute>() != null)
            {
                return false;
            }

            return field.IsPublic || field.GetCustomAttribute<SerializeFieldAttribute>() != null;
        }

        private static ScriptFieldType Classify(Type type, out AssetType assetType)
        {
            assetType = AssetType.Invalid;

            if (type == typeof(bool)) return ScriptFieldType.Boolean;
            if (type == typeof(int)) return ScriptFieldType.Integer;
            if (type == typeof(float)) return ScriptFieldType.Float;
            if (type == typeof(Vector2)) return ScriptFieldType.Vector2;
            if (type == typeof(Vector3)) return ScriptFieldType.Vector3;
            if (type == typeof(Vector4)) return ScriptFieldType.Vector4;
            if (type == typeof(string)) return ScriptFieldType.String;
            if (type == typeof(Entity)) return ScriptFieldType.Entity;

            if (!typeof(Asset).IsAssignableFrom(type))
            {
                return ScriptFieldType.Invalid;
            }

            // An asset field is nearly always declared as the concrete asset (Model, Material, ...)
            // rather than as the base. Each of those classes is named exactly as the engine names
            // the asset type, which is the same rule the object factory creates them by, so the
            // editor offers exactly the assets C# can be handed. A class the engine has no type for
            // still reflects as an asset field, with no type to restrict its picker to.
            Enum.TryParse(type.Name, false, out assetType);

            return ScriptFieldType.Asset;
        }

        // The types below are copied raw off the field, so a managed layout that is not the size the
        // engine stores would write past the engine's buffer or store the wrong bytes.
        //
        // Every other type is measured by something other than the field's own layout, and asking
        // for that layout would throw: bool is deliberately stored narrower than it marshals, an
        // asset field stores the UId behind a reference rather than the reference, and the rest are
        // variable-length or not stored at all.
        private static bool HasExpectedLayout(FieldInfo field, ScriptFieldType type)
        {
            switch (type)
            {
                case ScriptFieldType.Integer:
                case ScriptFieldType.Float:
                case ScriptFieldType.Vector2:
                case ScriptFieldType.Vector3:
                case ScriptFieldType.Vector4:
                    break;

                default:
                    return true;
            }

            var size = Marshal.SizeOf(field.FieldType);

            if (size == FixedValueSize(type))
            {
                return true;
            }

            Log.Error(field.DeclaringType.FullName + "." + field.Name + " is " + size
                      + " bytes, but the engine stores a " + type + " as " + FixedValueSize(type)
                      + ". The field will not be reflected.");

            return false;
        }
    }
}

using System;
using System.Runtime.InteropServices;

namespace Pine.Core.Reflection
{
    // The field types the engine is able to show and store. Mirrors Pine::ScriptFieldType, whose
    // values are written into saved components - append new ones, never renumber these.
    internal enum ScriptFieldType
    {
        Invalid = 0,
        Boolean = 1,
        Integer = 2,
        Float = 3,
        Vector2 = 4,
        Vector3 = 5,
        Vector4 = 6,
        Entity = 7,
        Asset = 8,
        String = 9
    }

    // The presentation choices that are a yes or no rather than a value. Mirrors
    // Pine::ScriptFieldFlags.
    [Flags]
    internal enum ScriptFieldFlags : uint
    {
        None = 0,
        HasRange = 1 << 0,
        HasSpace = 1 << 1
    }

    // One reflected field, in the form the engine reads it out of FieldRegistry. Member for member
    // and in order, this is Pine::ScriptFieldDescriptor in
    // Engine/src/Pine/Script/Scripts/ScriptFieldRegistry.hpp - the engine checks that the two agree
    // on size when the registry starts up, so a member added to one side is caught rather than
    // silently misread.
    //
    // Name, Header and Tooltip are UTF-8 strings allocated and owned by FieldRegistry. They stay
    // valid until the registry is reset, which happens on every reload; the engine copies out of
    // them as it reads each descriptor and never holds one of them.
    [StructLayout(LayoutKind.Sequential)]
    internal struct ScriptFieldDescriptor
    {
        public IntPtr Name;
        public int Type;
        public int AssetType;
        public uint Flags;
        public float RangeMin;
        public float RangeMax;
        public IntPtr Header;
        public IntPtr Tooltip;
    }
}

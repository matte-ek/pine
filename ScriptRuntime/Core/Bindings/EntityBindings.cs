using System;

namespace Pine.Core.Bindings
{
    // The engine functions behind Pine.World.Entity and Pine.World.EntityList.
    //
    // Two shapes are worth reading twice. A predicate answers with a byte rather than a bool,
    // because the engine returns a one-byte C++ bool and the default marshalling of a managed
    // bool is four bytes wide - the upper three would be whatever happened to be in the register.
    // And anything that would be an object answers with a handle: the engine cannot hand over a
    // managed reference, so Interop.ObjectFrom turns it back into one on this side.
    internal static unsafe class EntityBindings
    {
        public static delegate* unmanaged<uint, IntPtr> GetName;
        public static delegate* unmanaged<uint, byte*, void> SetName;
        public static delegate* unmanaged<uint, byte> GetActive;
        public static delegate* unmanaged<uint, byte, void> SetActive;
        public static delegate* unmanaged<uint, byte> GetStatic;
        public static delegate* unmanaged<uint, byte, void> SetStatic;
        public static delegate* unmanaged<uint, ulong> GetTags;
        public static delegate* unmanaged<uint, ulong, void> SetTags;
        public static delegate* unmanaged<uint, int> GetChildCount;
        public static delegate* unmanaged<uint, int, ulong> GetChild;
        public static delegate* unmanaged<uint, ulong> GetTransform;
        public static delegate* unmanaged<uint, int, byte> HasComponent;
        public static delegate* unmanaged<uint, int, ulong> GetComponent;
        public static delegate* unmanaged<uint, int, int> GetComponentCount;
        public static delegate* unmanaged<uint, int, int, ulong> GetComponentAt;
        public static delegate* unmanaged<uint, int, ulong> AddComponent;
        public static delegate* unmanaged<byte*, ulong> CreateEntity;
        public static delegate* unmanaged<uint, void> DestroyEntity;

        public static delegate* unmanaged<byte*, ulong> FindByName;
        public static delegate* unmanaged<ulong, int> FindByTagCount;
        public static delegate* unmanaged<ulong, int, ulong> FindByTagAt;
        public static delegate* unmanaged<int> GetCount;
        public static delegate* unmanaged<int, ulong> GetAt;

        public static void Bind()
        {
            GetName = (delegate* unmanaged<uint, IntPtr>)Interop.Resolve("Pine.World.Entity::GetName");
            SetName = (delegate* unmanaged<uint, byte*, void>)Interop.Resolve("Pine.World.Entity::SetName");
            GetActive = (delegate* unmanaged<uint, byte>)Interop.Resolve("Pine.World.Entity::GetActive");
            SetActive = (delegate* unmanaged<uint, byte, void>)Interop.Resolve("Pine.World.Entity::SetActive");
            GetStatic = (delegate* unmanaged<uint, byte>)Interop.Resolve("Pine.World.Entity::GetStatic");
            SetStatic = (delegate* unmanaged<uint, byte, void>)Interop.Resolve("Pine.World.Entity::SetStatic");
            GetTags = (delegate* unmanaged<uint, ulong>)Interop.Resolve("Pine.World.Entity::GetTags");
            SetTags = (delegate* unmanaged<uint, ulong, void>)Interop.Resolve("Pine.World.Entity::SetTags");
            GetChildCount = (delegate* unmanaged<uint, int>)Interop.Resolve("Pine.World.Entity::GetChildCount");
            GetChild = (delegate* unmanaged<uint, int, ulong>)Interop.Resolve("Pine.World.Entity::GetChild");
            GetTransform = (delegate* unmanaged<uint, ulong>)Interop.Resolve("Pine.World.Entity::GetTransform");
            HasComponent = (delegate* unmanaged<uint, int, byte>)Interop.Resolve("Pine.World.Entity::HasComponent");
            GetComponent = (delegate* unmanaged<uint, int, ulong>)Interop.Resolve("Pine.World.Entity::GetComponent");
            GetComponentCount = (delegate* unmanaged<uint, int, int>)Interop.Resolve("Pine.World.Entity::GetComponentCount");
            GetComponentAt = (delegate* unmanaged<uint, int, int, ulong>)Interop.Resolve("Pine.World.Entity::GetComponentAt");
            AddComponent = (delegate* unmanaged<uint, int, ulong>)Interop.Resolve("Pine.World.Entity::AddComponent");
            CreateEntity = (delegate* unmanaged<byte*, ulong>)Interop.Resolve("Pine.World.Entity::CreateEntity");
            DestroyEntity = (delegate* unmanaged<uint, void>)Interop.Resolve("Pine.World.Entity::DestroyEntity");

            FindByName = (delegate* unmanaged<byte*, ulong>)Interop.Resolve("Pine.World.EntityList::FindByName");
            FindByTagCount = (delegate* unmanaged<ulong, int>)Interop.Resolve("Pine.World.EntityList::FindByTagCount");
            FindByTagAt = (delegate* unmanaged<ulong, int, ulong>)Interop.Resolve("Pine.World.EntityList::FindByTagAt");
            GetCount = (delegate* unmanaged<int>)Interop.Resolve("Pine.World.EntityList::GetCount");
            GetAt = (delegate* unmanaged<int, ulong>)Interop.Resolve("Pine.World.EntityList::GetAt");
        }
    }
}

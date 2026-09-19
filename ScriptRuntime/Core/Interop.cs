using System;
using System.Runtime.InteropServices;
using System.Text;
using Pine.Core.Bindings;

namespace Pine.Core
{
    // The boundary between C# and the engine.
    //
    // Nothing here is reached by a game's scripts. It is the plumbing underneath everything they
    // do touch: the engine functions a binding calls, the strings those calls exchange, and the
    // handles that stand for managed objects the engine holds on to.
    internal static unsafe class Interop
    {
        // Bind Pine.dll to the engine, and answer whether it worked. The first thing the engine
        // asks of managed code, and the only entry point it can reach before this has run.
        //
        // resolveBinding is the engine's one way in: every binding in Bindings/ is a function it
        // hands back, looked up by the same name the engine registered it under. A name the
        // engine does not know means the two are built against different versions of each other,
        // which is not something to carry on from - scripting is reported off instead.
        [UnmanagedCallersOnly]
        public static int Initialize(IntPtr resolveBinding)
        {
            try
            {
                _resolveBinding = (delegate* unmanaged<byte*, IntPtr>)resolveBinding;

                // Log first, so that a failure in any of the others has somewhere to be reported.
                LogBindings.Bind();
                EntityBindings.Bind();
                ComponentBindings.Bind();
                AssetBindings.Bind();
                InputBindings.Bind();
                PhysicsBindings.Bind();

                return 1;
            }
            catch (Exception exception)
            {
                TryLog(exception);

                return 0;
            }
        }

        // Collect whatever managed objects nothing refers to any more. The editor asks for this
        // after stopping play; see Pine::Script::Runtime::RunGarbageCollector.
        [UnmanagedCallersOnly]
        public static void RunGarbageCollector()
        {
            try
            {
                GC.Collect();
                GC.WaitForPendingFinalizers();
            }
            catch (Exception exception)
            {
                Log.Error("Failed to collect managed objects: " + exception);
            }
        }

        // The engine function registered under a name. Throws rather than answering null: there
        // is nothing sensible to call in place of a binding the engine does not have, and
        // Initialize turns scripting off over it.
        public static IntPtr Resolve(string name)
        {
            using var utf8 = new Utf8Scope(name);

            var binding = _resolveBinding(utf8.Pointer);

            if (binding == IntPtr.Zero)
            {
                throw new EntryPointNotFoundException(name);
            }

            return binding;
        }

        // Zero is the engine's "no object" - see Pine::Script::ObjectHandle. A handle whose object
        // is not a T comes back as null rather than throwing, matching what the binding that
        // returned a managed reference directly used to do.
        public static T ObjectFrom<T>(ulong handle) where T : class
        {
            if (handle == 0)
            {
                return null;
            }

            return GCHandle.FromIntPtr(new IntPtr((long)handle)).Target as T;
        }

        // A string the engine handed back. The engine owns the bytes and reuses them, so this
        // copies rather than holding on to them - see Pine::Script::Bindings::ReturnString.
        public static string StringFrom(IntPtr text)
        {
            return Marshal.PtrToStringUTF8(text) ?? string.Empty;
        }

        // A UTF-8 copy of a string, for the engine to read. Nothing here frees it - the caller
        // owns the allocation and decides how long it lives; see FieldRegistry, which keeps its
        // descriptor strings alive until the registry is reset. A string being passed to a
        // binding wants Utf8Scope instead.
        public static IntPtr AllocUtf8(string text)
        {
            var bytes = Encoding.UTF8.GetBytes(text ?? string.Empty);
            var allocated = Marshal.AllocHGlobal(bytes.Length + 1);

            Marshal.Copy(bytes, 0, allocated, bytes.Length);

            // The engine reads these as C strings, so the terminator is part of the contract.
            Marshal.WriteByte(allocated, bytes.Length, 0);

            return allocated;
        }

        // A string for the length of one call into the engine. Every binding that takes a string
        // passes one of these, because the engine reads UTF-8 and C# does not store it that way.
        public readonly ref struct Utf8Scope
        {
            public Utf8Scope(string text)
            {
                _allocated = Marshal.StringToCoTaskMemUTF8(text ?? string.Empty);
            }

            public byte* Pointer => (byte*)_allocated;

            public void Dispose()
            {
                Marshal.FreeCoTaskMem(_allocated);
            }

            private readonly IntPtr _allocated;
        }

        private static delegate* unmanaged<byte*, IntPtr> _resolveBinding;

        // Binding failed, so logging may be one of the things that is broken. Worth attempting
        // anyway: the engine only knows that Pine.dll said no, and this is the only place the
        // reason exists.
        private static void TryLog(Exception exception)
        {
            try
            {
                Log.Error("Failed to bind Pine.dll to the engine: " + exception);
            }
            catch (Exception)
            {
                // Nothing left to report it with.
            }
        }
    }
}

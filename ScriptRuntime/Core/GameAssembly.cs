using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Runtime.Loader;
using Pine.World.Components;

namespace Pine.Core
{
    // The game's own assembly, and the script classes in it.
    //
    // It is loaded into a collectible context of its own so that rebuilding a script can replace
    // it while the editor is running. Everything the engine knows about a class - its type, its
    // lifecycle methods - is held here rather than there, because a reference the engine held
    // would keep the old assembly alive and turn every reload into a leak.
    //
    // Every method the engine calls is an entry point across a native boundary, so none of them
    // may let an exception escape: each one catches, logs and answers with a failure value.
    internal static unsafe class GameAssembly
    {
        // Replace whatever was loaded before, answering 1 when the assembly is ready to resolve
        // classes out of. Unload() is the engine's job to call first; it has its own references
        // to drop in between.
        [UnmanagedCallersOnly]
        public static int Load(byte* path)
        {
            try
            {
                var file = Interop.StringFrom((IntPtr)path);

                LoadIntoContext(file);

                return 1;
            }
            catch (Exception exception)
            {
                Log.Error("Failed to load the game assembly: " + exception);

                return 0;
            }
        }

        [UnmanagedCallersOnly]
        public static void Unload()
        {
            try
            {
                var unloaded = UnloadContext();

                if (unloaded == null)
                {
                    return;
                }

                // Unloading is cooperative: it finishes only once every reference into the
                // assembly is gone and a collection has run, and does nothing at all otherwise.
                //
                // How many collections that takes is not fixed. Some of what holds a script class
                // is freed by a finalizer rather than by the collector directly - reflection emits
                // an invoke stub for a method it has been asked for often enough, and that stub is
                // finalized - so each such layer costs a cycle of its own before the one underneath
                // it becomes unreachable. A script whose OnUpdate has run a few frames reliably
                // needs three. Ask again until it is gone rather than guessing the number.
                for (var attempt = 0; attempt < UnloadAttempts && unloaded.IsAlive; attempt++)
                {
                    GC.Collect();
                    GC.WaitForPendingFinalizers();
                }

                if (unloaded.IsAlive)
                {
                    Log.Warning("The previous game assembly did not unload - something is still "
                                + "holding a reference into it. It will stay in memory for the "
                                + "rest of this session, and so will the one after each reload.");
                }
            }
            catch (Exception exception)
            {
                Log.Error("Failed to unload the game assembly: " + exception);
            }
        }

        // Register the class a script asset names, and return the id the engine addresses it by,
        // or -1. A class that is not a script is not one of these: without Script behind it there
        // is no component identity to write into an instance of it.
        [UnmanagedCallersOnly]
        public static int ResolveClass(byte* namespaceName, byte* className)
        {
            try
            {
                if (_assembly == null)
                {
                    return -1;
                }

                var fullName = Interop.StringFrom((IntPtr)namespaceName) + "."
                               + Interop.StringFrom((IntPtr)className);

                var type = _assembly.GetType(fullName, false);

                if (type == null)
                {
                    return -1;
                }

                if (!typeof(Script).IsAssignableFrom(type))
                {
                    Log.Warning(fullName + " does not derive from Pine.World.Components.Script, "
                                + "so the engine cannot run it as a script.");

                    return -1;
                }

                Classes.Add(new ScriptClass(type));

                return Classes.Count - 1;
            }
            catch (Exception exception)
            {
                Log.Error("Failed to resolve a script class: " + exception);

                return -1;
            }
        }

        // Which lifecycle methods a registered class has, as the flags the engine mirrors in
        // Pine::Script::GameAssembly. None of them when it could not be read at all, which reads
        // as a class with nothing to dispatch.
        [UnmanagedCallersOnly]
        public static int GetDeclaredMethods(int classId)
        {
            try
            {
                var scriptClass = ClassOf(classId);

                if (scriptClass == null)
                {
                    return 0;
                }

                var methods = ScriptMethods.None;

                if (scriptClass.OnStart != null)
                {
                    methods |= ScriptMethods.OnStart;
                }

                if (scriptClass.OnUpdate != null)
                {
                    methods |= ScriptMethods.OnUpdate;
                }

                if (scriptClass.OnRender != null)
                {
                    methods |= ScriptMethods.OnRender;
                }

                return (int)methods;
            }
            catch (Exception exception)
            {
                Log.Error("Failed to read a script class' lifecycle methods: " + exception);

                return 0;
            }
        }

        // A handle to the class' Type, for the entry points that take one rather than an id. The
        // engine frees it through ObjectFactory.DisposeObject once the call is done.
        [UnmanagedCallersOnly]
        public static ulong NewClassTypeHandle(int classId)
        {
            try
            {
                var scriptClass = ClassOf(classId);

                if (scriptClass == null)
                {
                    return 0;
                }

                return (ulong)GCHandle.ToIntPtr(GCHandle.Alloc(scriptClass.Type)).ToInt64();
            }
            catch (Exception exception)
            {
                Log.Error("Failed to hand over a script class: " + exception);

                return 0;
            }
        }

        [UnmanagedCallersOnly]
        public static void InvokeOnStart(ulong objectHandle, int classId)
        {
            Invoke(objectHandle, classId, scriptClass => scriptClass.OnStart, null);
        }

        [UnmanagedCallersOnly]
        public static void InvokeOnUpdate(ulong objectHandle, int classId, float deltaTime)
        {
            // Reused rather than allocated per call: this runs once per script per frame, and the
            // engine dispatches from one thread.
            UpdateArguments[0] = deltaTime;

            Invoke(objectHandle, classId, scriptClass => scriptClass.OnUpdate, UpdateArguments);
        }

        [UnmanagedCallersOnly]
        public static void InvokeOnRender(ulong objectHandle, int classId, float deltaTime)
        {
            // Reused for the same reason as UpdateArguments.
            RenderArguments[0] = deltaTime;

            Invoke(objectHandle, classId, scriptClass => scriptClass.OnRender, RenderArguments);
        }

        // -------------------------------------------------------------------------------------

        [Flags]
        private enum ScriptMethods
        {
            None = 0,
            OnStart = 1 << 0,
            OnUpdate = 1 << 1,
            OnRender = 1 << 2
        }

        // How many collect-and-finalize cycles to give the unload before reporting it as failed.
        // Well above the three a reload is observed to need, because the cost of one more cycle on
        // an unload that was going to succeed anyway is a moment of a reload nobody is timing,
        // while a warning that is wrong sends someone hunting a leak that is not there.
        private const int UnloadAttempts = 10;

        private static readonly List<ScriptClass> Classes = new List<ScriptClass>();

        private static readonly object[] UpdateArguments = new object[1];
        private static readonly object[] RenderArguments = new object[1];

        private static GameLoadContext _context;
        private static Assembly _assembly;

        // Its own context, so that it can be replaced; collectible, so that replacing it actually
        // reclaims the old one.
        private sealed class GameLoadContext : AssemblyLoadContext
        {
            public GameLoadContext() : base("PineGame", true)
            {
            }

            // Nothing is resolved here. An assembly this context cannot find is looked for in the
            // default one next, which is where Pine.dll already is; answering with a copy sitting
            // beside Game.dll would load a second Pine, and the game's Script base class would
            // stop being the same type as the engine's.
            protected override Assembly Load(AssemblyName name)
            {
                return null;
            }
        }

        private sealed class ScriptClass
        {
            public ScriptClass(Type type)
            {
                Type = type;

                // Found on the class or on a script it derives from, and whatever their
                // accessibility: overriding a lifecycle method is what a script author is doing
                // here, and the engine has no reason to insist on a particular shape of it.
                OnStart = MethodOf(type, "OnStart", Type.EmptyTypes);
                OnUpdate = MethodOf(type, "OnUpdate", new[] { typeof(float) });
                OnRender = MethodOf(type, "OnRender", new[] { typeof(float) });
            }

            public Type Type { get; }
            public MethodInfo OnStart { get; }
            public MethodInfo OnUpdate { get; }
            public MethodInfo OnRender { get; }

            private static MethodInfo MethodOf(Type type, string name, Type[] parameters)
            {
                return type.GetMethod(name,
                    BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance,
                    null, parameters, null);
            }
        }

        // Loaded from the bytes rather than from the path: CoreCLR maps the file, and on Windows
        // a mapped Game.dll cannot be overwritten until its context has actually unloaded - which
        // would mean the build failing whenever the editor is open.
        [MethodImpl(MethodImplOptions.NoInlining)]
        private static void LoadIntoContext(string file)
        {
            var assemblyBytes = File.ReadAllBytes(file);
            var symbolsFile = Path.ChangeExtension(file, ".pdb");

            _context = new GameLoadContext();

            using var assemblyStream = new MemoryStream(assemblyBytes);

            if (!File.Exists(symbolsFile))
            {
                _assembly = _context.LoadFromStream(assemblyStream);

                return;
            }

            // Worth carrying: it is what puts line numbers in the stack trace of a script that
            // throws, which is the whole of what a gameplay author sees when one does.
            using var symbolStream = new MemoryStream(File.ReadAllBytes(symbolsFile));

            _assembly = _context.LoadFromStream(assemblyStream, symbolStream);
        }

        // Kept out of Unload so that no local of that frame is still referring to the context
        // when the collection below it runs - which would report a leak that is not one, or hide
        // one that is. Returns something to watch the unload through, or null if there was
        // nothing loaded.
        [MethodImpl(MethodImplOptions.NoInlining)]
        private static WeakReference UnloadContext()
        {
            Classes.Clear();

            if (_context == null)
            {
                return null;
            }

            var watch = new WeakReference(_context, true);

            _assembly = null;

            _context.Unload();
            _context = null;

            return watch;
        }

        private static ScriptClass ClassOf(int classId)
        {
            if (classId < 0 || classId >= Classes.Count)
            {
                Log.Error("The engine asked for script class " + classId + ", which is not registered.");

                return null;
            }

            return Classes[classId];
        }

        // Run one of a script's lifecycle methods. A script throwing is an ordinary thing for a
        // game under development to do, so it is reported with its stack trace and the frame
        // carries on - the alternative, across this boundary, is killing the editor.
        private static void Invoke(ulong objectHandle, int classId,
            Func<ScriptClass, MethodInfo> methodOf, object[] arguments)
        {
            ScriptClass scriptClass = null;

            try
            {
                scriptClass = ClassOf(classId);

                var instance = Interop.ObjectFrom<Script>(objectHandle);

                if (scriptClass == null || instance == null)
                {
                    return;
                }

                var method = methodOf(scriptClass);

                if (method == null)
                {
                    return;
                }

                method.Invoke(instance, arguments);
            }
            catch (TargetInvocationException exception)
            {
                Log.Error("Exception thrown in script '" + NameOf(scriptClass) + "': "
                          + exception.InnerException);
            }
            catch (Exception exception)
            {
                Log.Error("Failed to run a lifecycle method of script '" + NameOf(scriptClass)
                          + "': " + exception);
            }
        }

        private static string NameOf(ScriptClass scriptClass)
        {
            return scriptClass == null ? "<unknown>" : scriptClass.Type.FullName;
        }
    }
}

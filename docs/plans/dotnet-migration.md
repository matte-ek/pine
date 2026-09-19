# Moving scripting from Mono to modern .NET

Status: **units 1-6 (Part 3) are implemented**; unit 7 is not. The engine runs on CoreCLR.

Revised after a verification pass against the tree. The counts in Part 1 were re-checked and hold
(313 call sites, 13 files, 149 registrations against 149 `extern` declarations, and every row of
the split table bar two off-by-a-couple corrections). Six things did not hold and are now fixed in
place: `RayCastHit` cannot be made blittable (2.6), the field-registry teardown has to happen
earlier in the reload than an earlier draft said (2.5), managed reflection returns inherited
fields where Mono's did not (2.4), units 3–4 are stuck on C# 7.3 with no `[UnmanagedCallersOnly]`
(2.2), SDK-style conversion needs two more properties before anything loads (2.7), and `Game.dll`
types do not resolve through `Type.GetType()` (2.3). Open question 2 was pointing at a debugger
setup that does not exist.

**Recommendation up front: migrate, but do most of the work while still on Mono.** The end state is
CoreCLR hosted through `hostfxr`, with `Pine.dll` and each `Game.dll` targeting a current .NET.
The reason to sequence it this way is that four of the seven units below — the ones carrying the
real design risk — are changes to *where logic lives*, not to *which runtime runs it*. They can all
land on Mono, with a green build and a running editor after each one. By the time the runtime
actually swaps, the Mono surface left to replace is small and mostly mechanical.

That sequencing also puts the editor-attribute work (`[SerializeField]`, `[Range]`, `[Tooltip]`, …)
in **unit 3**, well before the runtime swap, because moving field reflection to the managed side is
a prerequisite for both and is worth having on its own.

Two things worth stating early, because they shape the risk:

- **The game-author-facing C# API does not change.** `data/projects/gm/assets/PlayerController.cs`
  compiles unmodified against the migrated `Pine.dll`. Every public member of `Pine.World.Entity`,
  `Pine.World.Components.Transform`, `Pine.Core.Log`, `Pine.Math.Vector3` and the rest keeps its
  signature. Only the `private static extern` plumbing behind those members changes.
- **The saved format does not change.** `Pine::ScriptFieldValue`, the pinned `Pine::ScriptFieldType`
  enum, and the `ScriptSerializer` payload in `Pine::ScriptComponent::SaveData` all stay exactly as
  they are. No `.passet` migration, no level re-save.

---

# Part 1 — What the Mono surface actually is

Measured, not estimated (re-counted September 2026): **313 `mono_` call sites across 13 files.**
Every one of them is under `Engine/src/Pine/Script/` except three calls in two files.

| File | Calls | What it does |
|---|---|---|
| `Script/Interfaces/ScriptInterfaceComponent.cpp` | 104 | Per-component property bindings |
| `Script/Interfaces/ScriptInterfaceEntity.cpp` | 50 | Entity properties, component lookup, arrays |
| `Script/Factory/ScriptObjectFactory.cpp` | 42 | Managed mirror construction |
| `Script/Scripts/ScriptField.cpp` (+2 in the `.hpp`) | 22 | Field classification + read/write |
| `Script/Interfaces/ScriptInterfaceInput.cpp` | 19 | Input bindings |
| `Script/ScriptManager.cpp` | 18 | Class resolution, field enumeration, dispatch |
| `Script/Runtime/ScriptingRuntime.cpp` | 18 | Host init, domain, assemblies, GC |
| `Script/Interfaces/ScriptInterfaceAsset.cpp` | 16 | Asset bindings |
| `Script/Interfaces/ScriptInterfaceLog.cpp` | 15 | Log bindings |
| `Script/Interfaces/ScriptInterfacePhysics.cpp` | 6 | Raycast |
| `World/Components/Script/ScriptComponent.cpp` | 2 | `mono_gchandle_get_target` |
| `Editor/.../ComponentPropertiesRenderer.cpp` | 1 | `mono_gchandle_get_target` |

**149 of the 313 are `mono_add_internal_call`** — registration, matching exactly 149
`private static extern` declarations across `ScriptRuntime/`. So slightly over half the surface is a
lookup table rather than logic.

## The part that is a rename, and the part that is a redesign

Split by whether CoreCLR has an equivalent:

| Mono call | Count | CoreCLR |
|---|---|---|
| `mono_add_internal_call` | 149 | No equivalent — becomes a function-pointer table handed to managed at boot (2.2) |
| `mono_gchandle_get_target` | 26 | Native stops dereferencing managed objects, but the eleven scalar mirror returns and the four mirror-array accessors hand back a handle instead (2.6) |
| `mono_field_get/set_value`, `mono_class_get_field_from_name` | 34 | **No C API at all.** Moves managed-side |
| `mono_string_to_utf8` / `_new` / `mono_free` | 26 | Becomes UTF-8 pointer marshalling |
| `mono_object_new`, `mono_runtime_object_init`, `mono_class_from_name` | 14 | **No C API at all.** Moves managed-side |
| `mono_array_new` / `_setref` / `_set` | 11 | **No C API at all.** Managed side allocates |
| `mono_class_get_fields`, `mono_field_get_name/type/flags`, `mono_type_get_name`, `mono_class_is_subclass_of` | 7 | **No C API at all.** Moves managed-side |
| `mono_runtime_invoke`, `mono_class_get_method_from_name` | 6 | Becomes a fixed set of managed entry points |
| Domain/assembly/GC lifecycle | 16 | `hostfxr` + `AssemblyLoadContext` |

The rows marked "no C API at all" are the migration. CoreCLR's hosting API hands you function
pointers into managed code and essentially nothing else — there is no supported native reflection,
object construction, field access or array allocation. Anything the C++ side currently does *to* a
managed object has to become something the C++ side *asks* managed code to do.

## Where Mono leaks past the seam

Four headers expose Mono types, which is why `ComponentPropertiesRenderer.cpp` in the Editor ended
up calling `mono_gchandle_get_target` directly:

| Header | Leak |
|---|---|
| `Script/Factory/ScriptObjectFactory.hpp` | `ObjectHandle::Object` is a `MonoObject*`; `GetEntityClass`/`GetComponentClass`/`GetRayCastHitClass` return `MonoClass*` (only the array-building bindings use them — they go with unit 2) |
| `Script/Scripts/ScriptData.hpp` | `MonoClass*`, `MonoMethod*`, `MonoClassField*` |
| `Script/Scripts/ScriptField.hpp` | `MonoClassField*`, `MonoClass*`, and `Get`/`Set` take `MonoObject*` |
| `Script/Runtime/ScriptingRuntime.hpp` | `MonoDomain*`, `MonoAssembly*`, `MonoImage*` |

`Pine::Script::ObjectHandle` then travels into `World/Entity/Entity.hpp`,
`World/Components/Component/Component.hpp`, `Assets/Asset/Asset.hpp` and
`World/Components/Script/ScriptComponent.hpp`. Those four only ever store and pass it, but because
the struct carries a `MonoObject*`, they all transitively depend on Mono headers — and the
`handle->Object != nullptr` idiom that appears throughout is a check on a Mono pointer.

This is the only part of the surface with a compounding cost, and unit 1 closes it.

---

# Part 2 — The target design

## 2.1 Runtime and hosting

**CoreCLR, hosted through `nethost`/`hostfxr`.** Target **`net10.0`**. The development machine has
both the 8.0 and 10.0 SDKs and runtimes installed, and .NET 8 leaves support in November 2026, so
there is no reason to start on it.

NativeAOT is not an option — it cannot hot reload, which rules it out on its own.

Boot becomes:

1. `get_hostfxr_path()` → load `libhostfxr`.
2. `hostfxr_initialize_for_runtime_config("engine/script/Pine.runtimeconfig.json")`.
3. `hostfxr_get_runtime_delegate(..., hdt_load_assembly, ...)` and load `engine/script/Pine.dll`
   with it. **This is the delegate that puts `Pine.dll` into `AssemblyLoadContext.Default`**, which
   2.5 depends on. The more commonly documented `hdt_load_assembly_and_get_function_pointer` must
   *not* be used here: it loads each assembly path into a private `IsolatedComponentLoadContext`,
   and the collectible game context (whose fallback is Default only) would then either fail to
   resolve `Pine` or load a second copy of it — at which point `Game.dll`'s `Script` base class is
   a different `Type` from the one the managed `ObjectFactory` knows.
4. `hostfxr_get_runtime_delegate(..., hdt_get_function_pointer, ...)` and call **one** managed
   bootstrap entry point in `Pine.dll`, which returns a struct of function pointers for everything
   else native needs.

Step 3 is belt-and-braces and worth keeping as such. `hdt_get_function_pointer` resolves through
`AssemblyLoadContext.Default`, whose assembly list this host initialisation builds from
`Pine.deps.json`, so step 4 alone would very likely pull `Pine.dll` into Default anyway. The
load-bearing half of the argument is the *negative* one — not
`hdt_load_assembly_and_get_function_pointer` — and that holds either way. Loading explicitly makes
the context `Pine.dll` lands in a stated decision rather than a side effect of probing order.

Step 4 matters: resolving managed methods by name string is slow, so pay it once for ~15–20 entry
points rather than per call. Everything native needs from managed fits in that struct — object
creation, disposal, script-class resolution, field enumeration, field get/set, and the
`OnStart`/`OnUpdate` dispatch. (`Pine::Script::Manager::OnRender` is an empty function today and
`ScriptData::MethodOnRender`/`MethodOnDestroy` are resolved but never invoked, so budget an entry
point for `OnRender` only if you intend to make it live.)

**`hdt_load_assembly` wants an absolute path.** `load_assembly_fn` documents its `assembly_path`
as *"Fully qualified path to assembly"*, so the cwd-relative `"engine/script/Pine.dll"` that
`mono_domain_assembly_open` accepts today has to be resolved against the working directory first.
`hostfxr_initialize_for_runtime_config` is more forgiving, but resolve both and keep the pair
together rather than remembering which one cares.

**A missing runtime must not stop the editor booting.** `Pine::Engine::Setup` ignores what
`Pine::Script::Runtime::Setup` returns (`Engine/src/Pine/Engine/Engine.cpp:95`), so a machine
without Mono today still opens the editor, with scripting off and `Runtime::IsAvailable()` false.
Keep that behaviour: a failed `get_hostfxr_path()` or `hostfxr_initialize_for_runtime_config`
logs and returns false, it does not abort. It matters more than it did, because the .NET runtime
is a separate install and this is the same code path GameHost hits on a player's machine.

`Pine::Script::RuntimeAssembly` loses `MonoAssembly*`/`MonoImage*` and becomes a path plus an
opaque managed handle. `Pine::Script::Runtime::GetDomain`, `GetPineAssembly` and `GetPineImage`
disappear from the header entirely — nothing outside `Script/` should be asking for them, and after
unit 1 nothing does.

## 2.2 The boundary: three kinds of crossing

**C# → native (149 bindings).** Recommend a **function-pointer table**: at boot, native fills a
struct of 149 function pointers and hands it to the managed bootstrap; each binding calls through a
`delegate* unmanaged<…>` field of that struct. Each `mono_add_internal_call("Pine.X::Y", Y)` line
becomes one assignment into the struct, so the existing `Setup()` tables in `ScriptInterface*.cpp`
keep their shape and their one-line-per-binding form.

```csharp
// ScriptRuntime/Core/Interop.cs — filled once by the bootstrap entry point.
internal static unsafe class Interop
{
    public static delegate* unmanaged<uint, float, void> AudioSource_SetVolume;
    // ...
}

// In the binding:
private static void PineSetVolume(uint id, float volume) => Interop.AudioSource_SetVolume(id, volume);
```

The obvious-looking alternative — `[LibraryImport]` against the host executable with
`NativeLibrary.SetDllImportResolver` returning `NativeLibrary.GetMainProgramHandle()` — is *not*
recommended, for a reason specific to this repo: `Engine` is a **static** library
(`add_library(Engine ${SOURCES})` in `Engine/CMakeLists.txt`). `ENABLE_EXPORTS ON` exports only the
symbols that make it into the executable, and a static archive's object files are pulled in only
when something references them. Today every `ScriptInterface*.cpp` is anchored by its `Setup()`,
whose body is nothing but `mono_add_internal_call` lines. Delete those and nothing references the
translation unit, the linker drops it, and every binding in that file fails at first call with
`EntryPointNotFoundException`. Working around it (`$<LINK_LIBRARY:WHOLE_ARCHIVE,Engine>`, or an
export macro plus a referenced anchor per file, plus `__declspec(dllexport)` on Windows) costs more
than the table does, and the table keeps the linker out of it entirely.

**Marshalling rules that apply to every binding, whichever route.** These are the places where "one
line each side" is not literally true; put them on the conversion checklist:

- **`bool` is not one byte across P/Invoke.** 24 of the 149 bindings take or return `bool`
  (`GetActive`, `SetActive`, `GetHasEntity`, `PineIsKeyDown`, `PineGetCastShadows`, …), and the C++
  side declares them as `bool`, which matches Mono's 1-byte `MonoBoolean`. P/Invoke's default for
  `bool` is the 4-byte Win32 `BOOL`, so a C++ `bool` return in `al` read as a 4-byte `BOOL` picks up
  whatever is in the upper bytes of `eax`. With the function-pointer table the fix is to declare
  those 24 as `byte` on the managed side and convert at the call (`!= 0`); with `[LibraryImport]` it
  is `[MarshalAs(UnmanagedType.U1)]`. Either way it is a deliberate edit at 24 sites, not a rename.
- **Struct parameters are fine as they are.** `Vector2/3/4` by `ref`/`out` (27 declarations, 28
  parameters — `PineWorldToScreenPoint` has one of each), `Vector3` by value in `RayCast`, and
  `UId` by value (two `ulong`s) all follow the platform ABI today and keep
  doing so. One tidy-up while passing through: `Pine.Core.UId` carries
  `[StructLayout(LayoutKind.Sequential)]` but `Pine.Math.Vector2/3/4` do not, and the vector types
  are auto-properties rather than fields. They are blittable regardless — C# lays structs out
  sequentially unless told otherwise, and the backing fields are floats — but
  `delegate* unmanaged<…>` *requires* blittability, so stating it on the type costs nothing and
  records the constraint where someone might otherwise add a `string` field to `Vector3`.
- **Strings** — see 2.6.

**Native → managed (~15–20 entry points).** The struct from 2.1, each backed by an
`[UnmanagedCallersOnly]` static in `Pine.dll`. `[UnmanagedCallersOnly]` signatures must be blittable
only: no `string`, no `bool`, no arrays, no managed references. Pass `const char*`, `byte`, handles
and pointers to structs, exactly as `ScriptFieldDescriptor` in 2.4 does.

**No exception may leave one of those entry points.** There is no managed frame above an
`[UnmanagedCallersOnly]` method to unwind into, so an escaping exception is not an error the caller
sees — CoreCLR fails the process outright. Every entry point in the struct wraps its whole body in
`try`/`catch`, logs through the `Pine.Core.Log` binding, and returns a failure value (`0` for a
handle, `-1` for a count, `false` for a predicate). This is a blanket rule, not advice for the
dispatch entry points alone: a `Type.GetType()` that throws inside object creation crashes the
editor exactly as readily as one inside `OnUpdate`. Write the `catch` at the same time as the entry
point, not as a hardening pass afterwards.

**Neither shape exists until unit 5 retargets the csprojs — which decides how units 3 and 4 are
written.** `delegate* unmanaged<…>` is C# 9, and no csproj sets `<LangVersion>`, so `v4.7.2` gives
C# 7.3. `[UnmanagedCallersOnly]` is worse than a language feature: it is a .NET 5+ runtime feature
that Mono 6.12 does not have at all. So the managed side of units 3 and 4 is written as ordinary
static methods, reached from C++ with `mono_runtime_invoke` and marshalling by hand through
`IntPtr`, and unit 5 re-plumbs those ~15–20 entry points onto `hdt_get_function_pointer` plus
`[UnmanagedCallersOnly]`.

What survives that swap is everything that matters: the descriptor struct in 2.4, the
length-then-fill value crossing, the field registry itself, and the rule that nothing throws across
the boundary. Only how each entry point is *reached* changes, and the signatures were blittable
already because 2.4 designed them that way. Two consequences worth holding on to — write the
entry points blittable from the start even though Mono would tolerate richer signatures, and count
the re-plumbing in unit 5's estimate rather than discovering it there.
`<LangVersion>latest</LangVersion>` on the old-style csproj buys back the modern syntax while
still on Mono, but not the attribute, so it does not remove the second pass.

**Managed object identity.** `Pine::Script::ObjectHandle` collapses to a single opaque value:

```cpp
struct ObjectHandle
{
    std::uint64_t Handle = 0;

    bool IsValid() const { return Handle != 0; }
};
```

Native must never hold a raw managed object pointer under CoreCLR — the GC moves objects and there
is no pinning here. It holds a `GCHandle` (`GCHandle.Alloc` → `GCHandle.ToIntPtr` managed-side,
which maps cleanly onto what `mono_gchandle_new` did) and passes it back to managed whenever the
object itself is needed.

Consequence: every `handle->Object != nullptr` check becomes `handle->IsValid()`. Those appear in
`ScriptComponent.cpp`, `Component.cpp`, `Entity.cpp`, `Asset.cpp`, the `assert` in
`Components.cpp` (`Components::Destroy`), `ScriptManager.cpp` and the Editor's `RenderScript`.
Mechanical, but it reaches outside `Script/` — which is why it is unit 1's job, done once, on Mono.

## 2.3 The object factory inverts

`Script/Factory/ScriptObjectFactory.cpp` is built entirely on calls with no CoreCLR counterpart:
`mono_object_new`, `mono_runtime_object_init`, `mono_field_set_value`, `mono_class_from_name`,
`mono_class_get_field_from_name`, `mono_gchandle_new`.

The whole file moves into `Pine.dll` as a managed `ObjectFactory`. The C++ namespace
`Pine::Script::ObjectFactory` keeps its current function signatures — `CreateEntity`,
`CreateComponent`, `CreateAsset`, `CreateScriptObject`, `DisposeEntity`, `DisposeComponent`,
`DisposeObject` — and each one becomes a call through the entry-point struct. Callers outside
`Script/` do not change at all.

Two things improve on the way:

- `mono_class_from_name(pineImage, "Pine.World.Components", ComponentTypeToString(type))` becomes a
  managed `Type.GetType()` lookup that **logs the missing type name and returns a null handle**
  instead of silently returning null. It must not throw — see the entry-point rule in 2.2; the
  point is a named error in the log, not an exception.
  `docs/scripting.md` flags the current silent-failure mode — an asset class whose name does not
  match `AssetTypeToString` simply never gets a mirror, and everything downstream hands back `null`.
  That class of bug stops being silent.
- The per-type `MonoClassField*` caches (`ComponentTypeData`, `AssetTypeData`) become managed
  `FieldInfo` caches that survive a game-assembly reload, once 2.5 splits the load contexts.

**Script classes do not resolve the same way engine classes do.** The `Type.GetType()` above is
right for `Pine.dll` types, which live in Default. `ResolveScriptData` in `ScriptManager.cpp`
resolves the *game* class instead (`mono_class_from_name(m_GameAssembly->Image, namespaceName,
className)`), and once 2.5 puts `Game.dll` in a collectible context, `Type.GetType()` will not
find it — it probes Default and stops. That lookup has to go through the `Assembly` object the
game context hands back when it loads `Game.dll`. So the script-class-resolution entry point in
2.1's struct takes a namespace and a class name and answers from the game assembly specifically,
and it is the one entry point that must be re-pointed at the new assembly on every reload.

## 2.4 Field reflection moves managed-side — and attributes come with it

This is the unit that pays for the editor-attribute work.

Today the policy lives in native code: `ProcessScriptFields` in `ScriptManager.cpp` checks
`MONO_FIELD_ATTR_PUBLIC` and skips `Parent`/`Type` by name, and `ClassifyField` in `ScriptField.cpp`
matches on type-name strings. Reading a *custom attribute* from there would mean
`mono_custom_attrs_from_field` and hand-decoding the blob. Under CoreCLR it is not possible at all.

Managed-side, it is ordinary C#:

```csharp
// ScriptRuntime/Core/Reflection/FieldRegistry.cs
private static bool ShouldReflect(FieldInfo field)
{
    if (field.GetCustomAttribute<HideInInspectorAttribute>() != null)
    {
        return false;
    }

    return field.IsPublic || field.GetCustomAttribute<SerializeFieldAttribute>() != null;
}
```

> **Gotcha — enumerate declared fields only.** The predicate above is the easy half; *which* fields
> it is asked about is the half that changes behaviour. `mono_class_get_fields` returns only the
> fields a class declares itself, but `Type.GetFields(BindingFlags.Public | BindingFlags.Instance)`
> returns inherited ones too — and `Pine.World.Component` declares
> `public readonly Entity Parent` and `public readonly ComponentType Type`. The straightforward
> managed enumeration therefore picks up both, and every script in the properties panel grows a
> `Parent (Entity) — "Not supported yet"` row it never had.
>
> Pass `BindingFlags.DeclaredOnly` and walk the hierarchy yourself, up to but not including
> `Pine.World.Components.Script`, so that a script deriving from another script still reflects the
> base script's fields. `ProcessScriptFields` skips `Parent`/`Type` by name today; keep that skip
> as well — it costs two string compares and it catches the case where someone adds a third public
> field to `Pine.World.Component`.

The attribute set to add in `ScriptRuntime/Core/Attributes/`:

| Attribute | Effect |
|---|---|
| `[SerializeField]` | Reflect a private field |
| `[HideInInspector]` | Do not reflect a public field |
| `[Range(min, max)]` | Slider instead of a drag/input widget |
| `[Tooltip("…")]` | Hover text on the row |
| `[Header("…")]`, `[Space]` | Grouping in the properties panel |

**Crossing format.** Each reflected field produces a descriptor:

```cpp
struct ScriptFieldDescriptor
{
    const char* Name;
    std::int32_t Type;       // Pine::ScriptFieldType
    std::int32_t AssetType;  // Pine::AssetType, Invalid unless Type == Asset
    std::uint32_t Flags;     // HasRange, ...
    float RangeMin;
    float RangeMax;
    const char* Header;      // nullptr when absent
    const char* Tooltip;     // nullptr when absent
};
```

A flat struct rather than something extensible, deliberately: `Pine.dll` and the engine live in one
repo and are always built together, so there is no version skew to design around and adding an
attribute later is a two-sided edit with no compatibility cost. The thing that genuinely must stay
stable is the *saved* form — `Pine::ScriptFieldType` values and the `ScriptFieldValue` byte layout —
and none of that is touched.

**String ownership in the descriptor.** The three `const char*` fields are allocated managed-side
(`Marshal.StringToCoTaskMemUTF8`) and are valid only for the duration of the enumeration call.
Native copies them into `std::string` members of `Pine::ScriptField` immediately; managed frees
them when the call returns. Native never holds one of these pointers past the call.

**Value payloads cross separately, and one of them is variable-length.** The descriptor carries
metadata; the field's *value* is a second crossing, and it is the one this unit has to specify
rather than assume. `ScriptFieldValue::Data` is a `std::vector<std::byte>` whose length depends on
the type: fixed for `Boolean` (one byte), `Integer`, `Float`, `Vector2/3/4` and `Asset` (a `UId`),
but arbitrary for `String` — which today is `mono_string_to_utf8` on the way out and
`mono_string_new_len` on the way in, both in `ScriptField.cpp`.

So the read entry point is a length-then-fill pair, not a single call:

```cpp
// Returns the byte length for this field on this object, or -1 if it cannot be read.
std::int32_t ScriptField_MeasureValue(std::uint64_t objectHandle, std::int32_t fieldIndex);

// Fills up to capacity bytes; returns the number written, or -1.
std::int32_t ScriptField_ReadValue(std::uint64_t objectHandle, std::int32_t fieldIndex,
                                   std::byte* buffer, std::int32_t capacity);
```

Writing is one call taking a pointer and a length. `Pine::ScriptField::ReadValue` sizes its vector
from the first call and fills it with the second, so its own signature and its callers are
unaffected.

This is also where the `bool` width rule from the gotcha below is physically enforced: the managed
writer emits one byte for `Boolean`, whatever `Marshal.SizeOf` would say.

`Pine::ScriptField` keeps its public interface (`GetType`, `GetName`, `GetAssetType`, `ReadValue`,
`WriteValue`) and gains accessors for the new metadata. Internally it stops holding a
`MonoClassField*` and holds a **field index** into the managed registry for that class — stable for
the lifetime of a load, and cheaper than a name lookup per access.

`Pine::ScriptField::Get<T>` / `Set<T>` are gone by the time this unit starts: they are inline
templates wrapping `mono_field_get_value`/`mono_field_set_value` in `ScriptField.hpp`, so unit 1
has to delete them to clear that header, and it converts their six callers in the Editor's
`RenderScriptField` (`Float`, `Integer`, `Boolean`, `Vector2`, `Vector3`, `Vector4` — only `String`
and `Asset` used `ReadValue`/`WriteValue` already) at the same time. What is left for this unit is
repointing `ReadValue`/`WriteValue` at the managed registry behind an unchanged signature, plus
rendering the new attribute metadata. That is what finally takes Mono out of the Editor.

> **Gotcha — `bool` width.** `Pine::ScriptFieldTypeSize` in `Script/Scripts/ScriptFieldValue.hpp`
> says `Boolean` is 1 byte, and saved data depends on it. C#'s `Marshal.SizeOf(typeof(bool))` is
> **4**. The managed writer must emit exactly one byte for a bool. Getting this wrong corrupts every
> saved script component with a bool field, and it will not be obvious.

## 2.5 Hot reload: AppDomain → collectible AssemblyLoadContext

`mono_domain_create_appdomain` / `mono_domain_unload` in `Pine::Script::Runtime::Setup` and
`Pine::Script::Runtime::Dispose` become an `AssemblyLoadContext` created with `isCollectible: true`
and torn down with `Unload()`.

**Split the two assemblies, which Mono does not currently do.** Today `Pine.dll` and `Game.dll`
share one appdomain and both are destroyed on every reload — which is why
`Pine::Script::ObjectFactory::Setup` re-resolves every class and field each time, and why
`GetAssetBaseClass` in `ScriptField.cpp` carries the comment *"a MonoClass does not survive a domain
reset"*. After the split:

- `Pine.dll` → **default** ALC, loaded once, never unloaded.
- `Game.dll` → **collectible** ALC, replaced per reload.

That has a large, good consequence. The managed mirrors for `Entity`, `Component` and `Asset` are
`Pine.dll` types, so they live in the default ALC and **do not need to be destroyed on reload at
all.** Only `ScriptComponent::m_ScriptObjectHandle` points at a `Game.dll` type.

It also has a workflow consequence that should be stated rather than discovered: **a rebuilt
`Pine.dll` is no longer picked up by hot reload.** Today `Pine::Script::Runtime::Reset` tears down
the whole appdomain, so rebuilding `Pine.dll` and refocusing the editor reloads it along with
`Game.dll`. After the split, `Pine.dll` is loaded once and engine-side C# changes need an editor
restart. That is the right trade — `Pine.dll` changes are engine development, not gameplay
iteration — but it is a change to how the editor behaves.

Load `Game.dll` into the collectible context with `LoadFromStream` (dll and pdb bytes), not
`LoadFromAssemblyPath`. CoreCLR maps the file, and on Windows a mapped `Game.dll` stays locked until
the context has actually unloaded, which means `msbuild` cannot overwrite it while the editor is
running. Reading the bytes first costs nothing on Linux and avoids rewriting the reload flow when
Windows is attempted.

**Resolve `Pine` from Default, and make sure nothing offers a second copy.** 2.1 closes one route to
two `Pine` assemblies; the build closes the other. Each `Game.csproj` references `Pine.dll` by
`HintPath`, and `Reference` is CopyLocal by default, so `Pine.dll` is copied into the project's
`runtime-bin/` beside `Game.dll` — and SDK-style conversion adds a `Game.deps.json` naming it. Hand
the collectible context an `AssemblyDependencyResolver` over that `deps.json`, which is the standard
plugin recipe and what `<EnableDynamicLoading>` is shaped to feed, and it resolves `Pine` from the
local copy and loads it a second time. `Game.dll`'s `Script` base class is then a different `Type`
from the one the managed `ObjectFactory` knows: the same failure as 2.1's, reached from the other
side. Two rules, both one line:

- `<Private>false</Private>` on the `Pine` reference in every `Game.csproj`, so nothing is copied.
- **No `AssemblyDependencyResolver` for the game context.** Let its `Load` override return `null`
  and fall through to Default, which is where `Pine.dll` already is. `Game.dll` has no other
  dependencies to resolve; if it ever gains one, resolve that one explicitly rather than
  reintroducing a resolver that can also answer for `Pine`.

So `Pine::Script::Runtime::Dispose`, which currently walks every entity, every component and every
asset tearing down handles — and `Setup`, which walks them all again rebuilding — shrinks to
capturing field values and destroying instances for `ScriptComponent` alone. Reload gets faster and
markedly simpler, and `Pine::Asset::InvalidateScriptHandle` plus the lazy rebuild in
`Pine::Asset::GetScriptHandle` may stop being needed.

`Pine::Script::Runtime::Setup` splits along the same line, and more sharply than `Dispose` does.
It currently runs the whole sequence on *every* reload — `mono_domain_create_appdomain`, opening
`Pine.dll`, the six `Interfaces::*::Setup()` registration tables, `ObjectFactory::Setup()`, then
rebuilding a script handle for every entity and component — because `Reset()` is `Dispose()`
followed by `Setup()`. Afterwards, the host, `Pine.dll` and the function-pointer table are all
boot-once, and the only thing a reload creates is a fresh collectible context. The `m_IsReloading`
flag and its rebuild loop go away with it, as does the `m_ComponentTypeLookupMap.clear()` that
`Interfaces::Entity::Setup` opens with — that one in unit 2, along with the map.

**The danger, stated plainly.** The comment in `Pine::Script::Runtime::Dispose` today reads:

> *The appdomain is about to be unloaded, which frees every GC handle wholesale.*

**That becomes false, and it is the single riskiest assumption in the migration.** An ALC unload is
cooperative and asynchronous: it completes only once every reference into it is gone — including
every `GCHandle` — and a GC has run. Miss one handle and the `Unload()` silently does nothing,
leaving a dead ALC, its assembly, and its JIT-compiled code in memory. Reload thirty times while
iterating on a script and the editor is holding thirty of them.

Two mitigations, both cheap, both worth doing:

1. Free every `Game.dll`-typed `GCHandle` explicitly before `Unload()` — which the existing
   `Dispose` structure already does for script objects, so the shape is right; it just goes from
   belt-and-braces to load-bearing.
2. **Add an unload diagnostic.** Keep a `WeakReference` to the ALC, force two `GC.Collect()` +
   `WaitForPendingFinalizers()` cycles after `Unload()`, and `PWarning` if it is still alive. This
   turns a silent leak into a visible message the moment someone introduces it.

Also audit for `Pine.dll` statics holding `Game.dll` references — a cached script instance anywhere
in the default ALC pins the collectible one open. `Pine.World.Entity.GetScript<T>()` returns its
result transiently today and is fine. **The biggest pin by design is the `FieldRegistry` from 2.4:**
it exists to hold `Type`, `MethodInfo` and `FieldInfo` for `Game.dll` classes (it is what replaces
`ScriptData`'s `MonoClass*`/`MonoMethod*`/`MonoClassField*`), and every one of those references
keeps the collectible context alive. The managed `ObjectFactory` caches from 2.3 are the second
place to look; they must key `Game.dll` types weakly or be cleared at the same point.

**Clearing them means reordering the reload, which is a change in its own right.** The call order
today is:

```
Pine::Script::Manager::ReloadGameAssembly
  ├─ Pine::Script::Runtime::Reset
  │    ├─ Pine::Script::Runtime::Dispose    <- the domain unload happens here
  │    └─ Pine::Script::Runtime::Setup
  ├─ Pine::Script::Manager::LoadGameAssembly
  └─ Pine::Script::Manager::ReloadScripts   <- deletes the old ScriptData, after the unload
```

`Pine::Script::Manager::ReloadScripts` is where the old `ScriptData` — and therefore, after unit 3,
the registry entries that replace its `MonoClass*`/`MonoMethod*`/`MonoClassField*` — is torn down,
and it runs *after* the unload. Under Mono that is harmless, because a stale `MonoClass*` is simply
dropped on the floor. Against a collectible ALC it is the exact failure this section is about: the
references are still live when `Unload()` is called, so the unload quietly does nothing.
`ReloadScripts` cannot just absorb the teardown either — `Pine::Engine::Run` calls it on its own at
`Engine/src/Pine/Engine/Engine.cpp:166`, just before the main loop starts, with no unload anywhere
near it.

So unit 5 splits it. The teardown half — `ScriptData`, the field registry and the managed
`ObjectFactory` caches — moves ahead of `Unload()`, either into `Pine::Script::Runtime::Dispose`
beside the `ScriptComponent` handle teardown already there, or into
`Pine::Script::Manager::ReloadGameAssembly` before it calls `Pine::Script::Runtime::Reset`.
`Pine::Script::Manager::ReloadScripts` keeps only the rebuild half, which is all the
`Pine::Engine::Run` caller ever wanted from it. Put the teardown in `Dispose` if you want one
place that is correct for every path into an unload; put it in `ReloadGameAssembly` if you would
rather `Dispose` stay a teardown of engine-side state only. Either works — deciding by accident does not.

## 2.6 Marshalling: the four shapes that change

**Strings.** Incoming: the table entry takes a `byte*`, and the binding encodes the `string` to
UTF-8 (`Marshal.StringToCoTaskMemUTF8`, freed after the call, or a `stackalloc` buffer for short
names) before calling through. Native receives a `const char*` directly, deleting
`mono_string_to_utf8` + `mono_free` at 14 sites. A small `Utf8Scope` helper in `Interop.cs` keeps
each binding to one line.

Returning is the one to get right. Native must never hand managed a buffer that managed will free —
which is exactly what `[DllImport]`'s default `string` return marshalling does. Return `IntPtr`
(`const char*`) and call `Marshal.PtrToStringUTF8` on the managed side, with native returning a
pointer into a `thread_local std::string` whose contents are valid until the next call to that
function. Only four sites return strings — `GetEntityName`, `GetFileName`, `GetPath` and
`GetInputBindName` — and all are main-thread, low-frequency.

**Arrays.** Five sites return `MonoArray*`: `Entity.Children`, `Entity.GetComponents`,
`EntityList.FindByTag`, `EntityList.GetAll`, and `Physics3D.RayCast`. Native cannot allocate a
managed array. Two patterns, chosen per site:

- *Arrays of managed mirrors* (the first four) → native exposes a count and an indexed accessor
  returning a handle; the managed side builds the array. Chattier, but simple and obviously correct.
- *`RayCast`* → count first, then a fill over a blittable **transport** struct that the managed
  binding converts. Not over `RayCastHit` itself: `Pine.Physics.Data.RayCastHit` carries
  `public Entity Entity`, a managed reference, so the struct is not blittable and cannot be made
  so while that field is a field. Native cannot write a managed reference into a span, and
  changing the field's type would break the game-facing API that decision 4 puts out of scope.
  So native fills a
  `RayCastHitRaw { std::uint64_t EntityHandle; Vector3f Position; Vector3f Normal; }` and the
  managed binding turns the span into a `RayCastHit[]`, materialising each `Entity` from its
  handle exactly as the eleven scalar mirror returns below do. The count has to come first for the
  same reason the string crossing in 2.4 needs `ScriptField_MeasureValue`: a fill call needs a
  capacity, and the hit count is not known until the query has run.

> **`RayCast` cannot be verified by running it, and that is not this migration's doing.**
> `PhysicsRayCast` in `ScriptInterfacePhysics.cpp` default-constructs a `physx::PxRaycastBuffer`,
> and `PxHitBuffer`'s constructor defaults to `aTouches = NULL, aMaxNbTouches = 0`. With no touch
> buffer PhysX reports only the blocking hit, `result.nbTouches` stays zero, and the binding always
> returns an empty array. So the one array conversion in unit 2 that the editor cannot exercise is
> this one. Give the buffer some touches first if you want unit 2 covered end to end; otherwise
> convert it by inspection and say in the unit-2 notes that it went untested.

**Managed mirrors returned singly.** Eleven more bindings return a `MonoObject*` that is not an
array: `GetTransform`, `GetComponent`, `AddComponent`, `CreateEntity` and `FindEntityByName`
(`ScriptInterfaceEntity.cpp`); `GetModel`, `AudioSourceGetAudioFile`, `ScriptGetCSharpScript` and
`ScriptGetInstance` (`ScriptInterfaceComponent.cpp`); `GetByPath` and `SpawnEntity`
(`ScriptInterfaceAsset.cpp`). Each is a `mono_gchandle_get_target(...->GetScriptHandle()->Handle)`
today, and each becomes: native returns the `std::uint64_t` handle, the managed binding materialises
it with `GCHandle.FromIntPtr(handle).Target` and casts. This is the bulk of what the Part 1 table
means by `mono_gchandle_get_target` disappearing — it is a deliberate signature change at eleven
sites, not a deletion. `ScriptGetInstance` is the one to look at twice: it is the only binding that
hands back a `Game.dll`-typed object, so it is the only one whose handle belongs to the collectible
context from 2.5.

**`typeof(T)`.** `Entity.HasComponent<T>` / `GetComponent<T>` / `GetComponents<T>` / `AddComponent<T>`
currently pass a `MonoReflectionType*` that `GetComponentType` in `ScriptInterfaceEntity.cpp` maps
back through `m_ComponentTypeLookupMap`. Native cannot inspect a `System.Type` under CoreCLR.

Resolve it managed-side instead and pass an `int`. The `Type` field `Pine.World.Component`
(`ScriptRuntime/World/Component.cs`) already carries is no help here — it is an instance field the
native factory writes at construction, so there is nothing to read from `T` without an object of
it. A static `ComponentTypeOf<T>()` needs its own source, and there are two: a
`Dictionary<Type, ComponentType>` built once in a static constructor, or an attribute on each
class in `ScriptRuntime/World/Components/`. Prefer the attribute — it keeps the mapping on the
class it describes, where someone adding a component will see it, rather than in a list two
directories away. Either way the generic signatures the game author sees are unchanged, and
`m_ComponentTypeLookupMap` plus `mono_reflection_type_get_type` are deleted.

**Exceptions.** `mono_runtime_invoke`'s out-parameter plus `mono_object_to_string` becomes an
ordinary `try`/`catch` in the managed dispatcher, logging through the existing `Pine.Core.Log`
binding. That gets you a real stack trace instead of `ToString()` on the exception object, which is
a straight improvement on what `Pine::Script::Manager::OnStart` and `OnUpdate` report today.

> Keep the per-component dispatch loop in C++ for now. Moving the whole `OnUpdate` iteration into
> managed would cut per-frame interop from O(components) to O(1), but it moves ownership of
> iteration order and of the ECS's enabled/disabled skipping across the boundary. Worth doing later,
> on its own, against a measurement — not folded into a migration whose value depends on behaviour
> staying identical.

## 2.7 Build and deployment

| Piece | Today | After |
|---|---|---|
| `ScriptRuntime/Pine.csproj` | Old-style, `<TargetFrameworkVersion>v4.7.2</TargetFrameworkVersion>` | SDK-style, `<TargetFramework>net10.0</TargetFramework>`, `<EnableDynamicLoading>true</EnableDynamicLoading>` |
| `data/projects/<name>/runtime/Game.csproj` | Same, globs `..\assets\**\*.cs` | Same conversion; keep the glob; `<Private>false</Private>` on the `Pine` reference (2.5) |
| `data/projects/project-template/runtime/Game.csproj` | Same | **Same conversion — this is the one to not forget.** Every project created after the migration is stamped from it |
| Build command | `msbuild -t:Build -p:Configuration=Release` | `dotnet build -c Release` |
| CMake (4 files) | `/usr/include/mono-2.0`, link `mono-2.0` | `nethost` headers + lib on `Engine`; nothing on the executables (the function-pointer table in 2.2 needs no symbol export) |
| Runtime config | — | `Pine.runtimeconfig.json` beside `Pine.dll` in `data/engine/script/` |

There are four csproj files in the tree, not two: `ScriptRuntime/Pine.csproj` and three
`Game.csproj` — `data/game/runtime/`, `data/projects/gm/runtime/` and
`data/projects/project-template/runtime/`. The first two are existing content and convert when they
are next built; the template is the one that matters, because missing it means every project
created after the migration is born targeting `v4.7.2` and fails at first load with no obvious
cause.

`<EnableDynamicLoading>true</EnableDynamicLoading>` is the detail that is easy to miss: without it a
class-library project emits no `runtimeconfig.json` or `deps.json`, and hosting cannot start.

**Two more properties are not optional either, and both of them bite on the first `dotnet build`.**
They apply to all four projects:

- `<GenerateAssemblyInfo>false</GenerateAssemblyInfo>`, or delete the four
  `Properties/AssemblyInfo.cs` files. Every project compiles one, and they carry
  `[assembly: AssemblyTitle]`, `AssemblyProduct`, `AssemblyVersion` and the rest — exactly the
  attributes an SDK-style project generates for itself. The build stops on
  `CS0579: Duplicate 'AssemblyTitleAttribute' attribute`. Deleting them is the tidier of the two:
  nothing in the tree reads those values.
- `<AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>`. SDK projects
  write to `<OutputPath>/<tfm>/`, and every path that loads one of these assemblies is hardcoded
  without a framework segment: `engine/script/Pine.dll` in `Pine::Script::Runtime::Setup`, and
  `<project>/runtime-bin/Game.dll` in `Editor/src/Application.cpp:51`,
  `Editor/src/Utilities/Scripts/ScriptUtilities.cpp:41` and `GameHost/src/Application.cpp:18`.
  This is the worst kind of miss: the build succeeds, the editor starts, and scripting is just
  silently absent.

The conversions are also not equally mechanical. The three `Game.csproj` keep their explicit
`<Compile>` for the `..\assets\**\*.cs` glob and are otherwise a property swap.
`ScriptRuntime/Pine.csproj` lists all 36 of its sources by hand, and SDK-style globbing replaces
that `ItemGroup` outright — so `Pine.csproj` is closer to a rewrite from a template than an edit.

The toolchain is already in place on the development machine: `dotnet` SDKs 8.0 and 10.0 with both
runtimes, alongside Mono 6.12 and `msbuild`, so units 1–4 and unit 5 can be built side by side.

The `nethost` headers (`nethost.h`, `hostfxr.h`, `coreclr_delegates.h`) and `libnethost` ship in the
SDK under `packs/Microsoft.NETCore.App.Host.<rid>/<version>/runtimes/<rid>/native/`. **Do not
hardcode `linux-x64` in the CMake lookup.** This machine's SDK is the distro-packaged one and its
pack is `Microsoft.NETCore.App.Host.arch-x64`, so a `find_path` spelling `linux-x64` finds nothing
here even though the template above is right. Glob the `<rid>` directory, or resolve it from
`dotnet --list-runtimes`, rather than naming it.

Two things get strictly better:

- `mono_config_parse("/etc/mono/config")` and its `// TODO: Figure out how we'll handle this on
  Winblows` disappear. There is no equivalent to port.
- Building `Pine.dll` no longer needs Mono installed — `dotnet build` is the same command on every
  platform.

**The open question is GameHost shipping.** Framework-dependent means players need a .NET runtime
installed; self-contained means bundling roughly 70 MB alongside the game. Recommend
framework-dependent for development and self-contained for anything shipped, decided when GameHost
packaging is next touched rather than during the migration.

---

# Part 3 — Sequencing

Seven units. **Units 1–4 run on Mono** — each ends with a green build and a working editor, and each
is worth having even if the migration stalls. Unit 5 is the only one that cannot be partial.

### Unit 1 — Take Mono out of the engine headers

Make `Pine::Script::ObjectHandle` opaque (`std::uint64_t` + `IsValid()`); replace every
`handle->Object != nullptr` with `handle->IsValid()`; remove Mono types from the public surface of
`ScriptField.hpp`, `ScriptData.hpp` and `ScriptingRuntime.hpp`; give the Editor an accessor so
`ComponentPropertiesRenderer.cpp` stops calling `mono_gchandle_get_target`.

**The `ScriptField` value accessors have to move in this unit too, and further than they first
look.** Two things force it:

- `Pine::ScriptComponent::CaptureFieldValues` and `Pine::ScriptComponent::ApplyFieldValues` in
  `World/Components/Script/ScriptComponent.cpp` call `mono_gchandle_get_target` and hand the
  resulting `MonoObject*` to `Pine::ScriptField::ReadValue`/`WriteValue`. The Editor is not the only
  caller outside `Script/`. So `ReadValue`/`WriteValue` take the `ObjectHandle` from here on, and
  do the dereference internally in `ScriptField.cpp`.
- `Pine::ScriptField::Get<T>`/`Set<T>` are inline templates in `ScriptField.hpp` whose bodies *are*
  `mono_field_get_value`/`mono_field_set_value`. No change of parameter type gets Mono out of that
  header while they exist, and moving them out of line would mean explicitly instantiating six
  types to no benefit. So **delete them in unit 1** and switch the Editor's six cases
  (`Float`, `Integer`, `Boolean`, `Vector2`, `Vector3`, `Vector4`) to `ReadValue`/`WriteValue`,
  which the `String` and `Asset` cases beside them already use.

That moves the `RenderScriptField` case conversion out of unit 3 and into unit 1, where it is
forced anyway. Unit 3 is then only about *where the implementation lives*, which is a cleaner split
than the one this plan originally drew.

After this, Mono appears in `Engine/src/Pine/Script/*.cpp` and nowhere else. `ScriptComponent.cpp`
loses its `<mono/metadata/object.h>` include along with the rest. No behaviour change.
*This is the unit that stops the cost compounding, and it is worth doing this month regardless of
when the rest happens.*

### Unit 2 — Delete the reflection-shaped bindings

Move `typeof(T)` → `ComponentType` resolution to managed (2.6); convert the five `MonoArray*`
returns to count+index or a blittable fill. Removes `mono_reflection_type_get_type`,
`m_ComponentTypeLookupMap` and all eleven `mono_array_*` calls **while still on Mono**, so the
riskiest signature changes are proven against a working runtime. No behaviour change.

### Unit 3 — Field reflection into `Pine.dll`, with attributes

The managed `FieldRegistry` (2.4), the attribute set, the descriptor struct, and the
length-then-fill value crossing. `Pine::ScriptField::ReadValue`/`WriteValue` keep the signatures
unit 1 gave them and change only underneath; `RenderScriptField` in the Editor needs no further
work beyond rendering the new attribute metadata, because unit 1 already converted its cases.
Native still calls it via `mono_runtime_invoke` for now, and the managed side is written in C# 7.3
against `v4.7.2` — see the language-version note in 2.2 for what that rules out and what it costs
unit 5.

**This is where the editor attributes ship.** Largest single unit, and the one with real user-facing
value independent of the runtime swap. Watch the `bool` width gotcha, and the `DeclaredOnly` one
beside it — managed reflection hands back inherited public fields where `mono_class_get_fields`
did not.

### Unit 4 — Object creation into `Pine.dll`

The managed `ObjectFactory` (2.3). `Pine::Script::ObjectFactory` keeps its signatures; its body
becomes calls into managed. After this the *only* remaining Mono usage is host init, assembly
loading, internal-call registration, string marshalling, and `mono_runtime_invoke` against a fixed
entry-point set. That is a little over 200 of the original 313 call sites, but 149 of them are the
registration table and ~26 are string marshalling, so what is left to *think* about is small.

**As built**, three things differ from the sketch above, all of them shrinking unit 5:

- The `mono_runtime_invoke` plumbing units 3 and 4 both need was extracted to
  `Script/ManagedCall.hpp` - entry-point lookup, the call itself, exception-to-log, and the
  temporary reflection-`Type` handle. `Pine::Script::FieldRegistry` moved onto it too, so unit 5
  re-points **one** seam rather than two files' worth of call sites.
- A component's managed class is found by the `[ComponentType]` attribute that
  `ComponentTypes.Of<T>` already reads, rather than by name. The two directions now answer from one
  attribute and cannot drift; the name rule survives only for assets, where it was already the
  documented convention. A type with no class is warned about once, which is what 2.3 asked for.
- Every component class inherits `Parent`, `Type`, `_internalId` and `_isValid` from
  `Pine.World.Component`, so there is no per-class `FieldInfo` cache to keep - one set covers
  every component and every script. `ScriptClassBinding` lost its three `MonoClassField*` members
  with it, and `ScriptData::IsReady` became the `mono_class_is_subclass_of` check those members
  were standing in for.

One caller outside `Script/` did change after all, against what this unit promised:
`ObjectFactory::DisposeComponent` dropped its `const Component*`, which only existed to find the
per-type field cache that no longer exists. `Pine::Component::DestroyScriptInstance` passes the
handle alone.

Handles are now allocated managed-side by `GCHandle.Alloc`, which makes them strong but **not**
pinned; `mono_gchandle_new` was being passed `pinned = true`. Nothing held an address, and
`GCHandle.Alloc(obj, GCHandleType.Pinned)` would throw on CoreCLR for these objects anyway.

### Unit 5 — The runtime swap

Replace `Pine::Script::Runtime` with `hostfxr` (`hdt_load_assembly` + `hdt_get_function_pointer`,
per 2.1); turn the six `Setup()` registration tables into the function-pointer table from 2.2 and
convert the 149 bindings to call through it; re-plumb the ~15–20 managed entry points units 3 and 4
built for `mono_runtime_invoke` onto `hdt_get_function_pointer` plus `[UnmanagedCallersOnly]`
(2.2); apply the `bool` rule at its 24 sites, the string rule at its 18, and the handle-return rule
at the eleven scalar mirror returns in 2.6; split default/collectible ALC, loading `Game.dll` from
a stream and resolving `Pine` from Default with no `AssemblyDependencyResolver`; move the
`ScriptData` and managed-cache teardown ahead of the unload, splitting
`Pine::Script::Manager::ReloadGameAssembly` (2.5); retarget all four csprojs, the template
included, with `GenerateAssemblyInfo` and `AppendTargetFrameworkToOutputPath` both off (2.7);
update the four `CMakeLists.txt`.

Big, but by this point mostly mechanical. **The two non-mechanical pieces are the ALC split and the
reload reordering it forces**, and the payoff is the simplification in 2.5. The `bool` and string
sites are mechanical too, but they are the two places where a straight rename compiles and then
misbehaves, so do them as a
deliberate pass with the list in hand rather than as part of the rename. The eleven handle returns
fail loudly rather than quietly, so they can ride along with the rename.

**As built**, seven things differ from the sketch above:

- **The C# → native table is resolved by name, not by position.** 2.2 asked for a struct of
  function pointers declared member-for-member on both sides; instead the engine keeps a
  name-to-pointer map (`Script/Bindings/`) that the six `Interfaces::*::Setup` tables fill -
  each `mono_add_internal_call` line became a `Bindings::Register` line with no other change -
  and `Pine.Core.Interop.Initialize` asks for each one back by that name. The reason is failure
  mode: two hand-maintained 155-entry structs that drift by one member misroute every call below
  it, silently, whereas a name that one side has and the other does not is a line in the log at
  startup. The bootstrap still happens exactly once, and the per-call cost is the same indirect
  call either way. It also makes the two lists checkable against each other with a grep, which is
  how the count above was confirmed: **155 bindings, not 149** - unit 2's count-and-index
  crossings added six.
- **There is no entry-point struct either.** 2.1 step 4 wanted one bootstrap call returning a
  struct of managed function pointers, to avoid paying name resolution per call. Resolving each of
  the ~22 entry points through `hdt_get_function_pointer` at boot pays it once too, and needs no
  second pair of structs to keep in step, so `ManagedCall::Find<Signature>` does that and the
  caller holds an ordinary typed function pointer. The one bootstrap call that remains is the one
  handing managed code the binding resolver.
- **`Pine::Script::Runtime::Reset` is gone rather than shrunk**, and the teardown went to
  `Pine::Script::Manager::ReloadGameAssembly` - 2.5's second option. Once `Pine.dll` and the host
  are boot-once, a reload has nothing to do with the runtime's lifecycle: it captures field values,
  destroys script instances, drops the `ScriptData` and the field registry, unloads, loads, rebuilds.
  All of that reads in one function. `Runtime::Dispose` stays a teardown of engine-side state only.
- **`ScriptClassBinding` is gone.** It existed to hold a `MonoClass*` and four `MonoMethod*`, none
  of which the engine may hold any more. `ScriptData` carries the class' id in the game assembly
  and two `bool`s saying which lifecycle methods it has, so the opaque-struct dance, its forward
  declaration and its heap allocation all go with it. The field registry keeps its own ids, which
  are not the game assembly's - `ProcessScriptFields` is the only place both are in scope.
- **Two latent bugs surfaced and are fixed**, both of which the old binding mechanism hid.
  `Pine.Input.InputManager` declared `PineGetMouseButtonState` while the engine registered
  `PineGetMouseButtonKeyState`, so `GetMouseButtonState` had no implementation at all under Mono;
  name resolution turns that from a runtime miss into a startup error, so the names now agree.
  And `Transform.Rotation` was declared `out Vector3` against a binding that writes a
  `Pine::Quaternion` - a 16-byte write into a 12-byte slot. It is a `Quaternion` now, matching
  `LocalRotation` beside it and the engine's own `Transform::GetRotation`. **This is the one
  game-facing C# signature the migration changes**, against decision 4; nothing in the tree uses
  it, and the alternative was keeping a property that corrupts the caller's stack.
- **The unload diagnostic from unit 6 landed here**, because unit 5 cannot be called done without
  it: a failed `Unload()` is silent by construction, so there is otherwise no way to tell a
  working reload from a leaking one. It is the `WeakReference` check 2.5 describes, in
  `Pine.Core.GameAssembly.Unload`.
- **Three edges outside the four csprojs and the four `CMakeLists.txt`** that 2.7's table does not
  list, and that a build stops working without: `setup-env.sh` installs `dotnet-sdk` where it
  installed `mono`; the four script probes in `Editor/src/DebugServer/Verification/` build their
  throwaway `Game.dll` with `dotnet build` where they used `mcs`; and `.gitignore`'s
  `assets/engine/script` - a path from before `assets/` was renamed `data/`, and so ignoring
  nothing - became `data/engine/script/`, which is where `Pine.dll` now lands alongside the
  `runtimeconfig.json` and `deps.json` the host needs.

### Unit 6 — Reload correctness

The `GCHandle` audit, the `WeakReference` unload diagnostic, the shrunk `Dispose`/`Setup`, and
rewriting the now-false comment in `Pine::Script::Runtime::Dispose`. Worth its own unit because it
is where the unpredictable debugging time lives — budget for iteration here, not elsewhere.

Unit 5 moves the teardown ahead of the unload; this unit is where you find out whether it moved
*everything*. Start from the diagnostic rather than from reading code: a `WeakReference` that
survives two collect-and-finalize cycles tells you there is a pin, and the audit is only worth
doing once it has told you that.

**As built**, three things differ from the sketch above:

- **Three of the four items had already landed in unit 5** - the diagnostic, the shrunk
  `Pine::Script::Runtime::Dispose`/`Setup`, and the comment that went with them - so what was
  actually left was proving the reload under cases harder than the one throwaway probe unit 5 ran.
  That is `verify-script-reload.py`, the fifth probe in `Editor/src/DebugServer/Verification/`:
  four script classes, one deriving from another, holding an entity, a component, an asset and an
  instance of another class in the same assembly, with a fifth that throws once per reload; play
  mode entered, thirty reloads in a row, and the scene changed between each of them.
- **The advice above was exactly backwards, and the two-cycle count was the bug.** The first
  reload of that probe reported a leak. It was not one: the context unloads on the *third*
  collect-and-finalize cycle, every time, and `Pine.Core.GameAssembly.Unload` only ever ran two.
  The extra cycle appears once a class' `OnUpdate` has been invoked about four times, which is
  where reflection stops interpreting the call and emits an invoke stub for it - and that stub is
  freed by a finalizer, so it costs a cycle of its own before the loader allocator underneath it
  becomes unreachable. Unit 5's probe never reached that threshold. The loop now asks again until
  the context is gone, up to ten times, rather than guessing the number; with the fix, thirty
  reloads warn not once and the resident set is flat (+944 kB over the whole run).
  **A diagnostic that cries wolf is worse than no diagnostic**, because the next person to see it
  goes looking for a pin that does not exist - which is most of a day.
- **No pin was found, and none of the audit's static half turned anything up either.** Every
  `GCHandle` the engine takes is paired: `Pine::Entity`'s with its constructor and destructor,
  `Pine::Component`'s with `OnCreated`/`OnDestroyed` (and `Components::Destroy` asserts it), the
  script instance's with `Pine::ScriptComponent::CreateInstance`/`DestroyInstance`, the asset's
  with `Pine::Asset::GetScriptHandle`/`~Asset`, and the class `Type`'s with
  `GameAssembly::ScopedClassType`. The `Pine.dll` statics that could hold a `Game.dll` reference
  are `Pine.Core.GameAssembly.Classes` and `Pine.Core.Reflection.FieldRegistry.Classes`, both
  cleared before `Unload()`; `Pine.Core.ObjectFactory`'s two class caches are built from
  `Pine.dll`'s own assembly and cannot contain one.

**The audit's one real finding was in shutdown, not in reload**, and it is fixed here with the
author's agreement. `Pine::Engine::Shutdown` called `Pine::Script::Runtime::Dispose` *first*, and
then `Entities::Shutdown`, `Components::Shutdown` and `Assets::Shutdown` freed every mirror through
managed code that `Dispose`'s own comment said must not be reached any more. It worked only because
`hostfxr_close` does not stop the runtime - the invariant was false, and the one place that
honoured it was `Pine::Asset::DestroyScriptHandle`, which checked
`Pine::Script::Runtime::IsAvailable()` and forgot the handle instead of freeing it.
`Pine::Script::Runtime::Dispose` now runs *after* those three, so every mirror is freed through a
live runtime and the comment is true. `Pine::Asset::InvalidateScriptHandle` existed for that case
alone and is gone with it, which is what 2.5 guessed would happen to it; the lazy create in
`Pine::Asset::GetScriptHandle` stays, because an asset touched before the runtime is up - or on a
machine with no .NET at all - still must not try to make one. The script probes all run the editor
to a real `Pine::Engine::Shutdown`, so the reordered teardown is exercised on every one of them.

### Unit 7 — Deployment and docs

GameHost packaging decision, a Windows build check, and rewriting `docs/scripting.md` — which
describes the Mono host, the appdomain reload and the msbuild commands throughout, and will be
wholesale wrong after unit 5.

## Dependencies

```
1 ──▶ 2 ──▶ 3 ──▶ 4 ──▶ 5 ──▶ 6 ──▶ 7
```

Strictly linear. 2 and 3 are independent of each other in principle, but both touch
`ScriptInterfaceEntity.cpp` and `ScriptManager.cpp` enough that interleaving them is not worth the
merge pain.

---

# Part 4 — What verification looks like

Units 2, 3, 4 and 6 each left a native probe behind in `Editor/src/DebugServer/Verification/`, and
between them they now cover most of this list without a person in front of the editor (unit 5
moved the first four off `mcs` and onto a throwaway SDK project built with `dotnet build`):
`verify-script-components.py` (the per-component bindings), `verify-script-collections.py` (the
count-then-index crossings), `verify-script-fields.py` (authoring, hot reload, play/stop and a level
round trip), `verify-script-lifetime.py` (a script still holding an entity and a component the
engine has destroyed) and `verify-script-reload.py` (thirty reloads in a row, checking the
assembly unloads every time). Each builds the Editor's own objects against a probe
`Application.cpp` and runs headless under Xvfb, so they are the cheapest thing to run after every
unit. Run all five.

Beyond them, verification is running the editor and the sample
project. `data/projects/gm/assets/PlayerController.cs` happens to be an excellent end-to-end probe —
it exercises `Parent.GetComponent<T>()` (the `typeof` path), `Parent.Children` (the array path),
`Transform` property get/set, `InputManager`, and `CharacterController` in one script, and its five
`public float` fields cover the fixed-size half of the field crossing.

What it does not cover, and what therefore needs a probe of its own: `bool`, `string` and `Asset`
fields (added below, from unit 3), `Physics3D.RayCast` (which returns nothing today — see the note
in 2.6), and `EntityList.FindByTag`/`GetAll`, the two array conversions in unit 2 that
`PlayerController.cs` never calls. A three-line script in the same project covers the last of
those.

After every unit:

1. Build the engine, the Editor and GameHost.
2. Build `Pine.dll` and the `gm` project's `Game.dll`.
3. Launch the Editor headless and confirm the game assembly loads with no warnings from
   `ResolveScriptData`. (See the note in agent memory on launching under Xvfb, and on not running
   from the repo's `data/`.)
4. Select an entity with a `ScriptComponent`, confirm the properties panel lists its fields with the
   right types — and, from unit 3, that it lists *only* those: no inherited `Parent` or `Type` row
   (2.4). Edit one and confirm the value survives save → load.
5. Enter play mode, confirm `OnStart`/`OnUpdate` run and movement works.
6. Touch `PlayerController.cs`, rebuild `Game.dll`, refocus the editor window, and confirm the hot
   reload completes with field values preserved.

Step 6 is the one that matters most from unit 5 onward — and from unit 6 it should also be run
**twenty or thirty times in a row** with the process RSS watched, because that is the only way the
ALC-unload failure mode shows itself. Unit 5 did one such run, from a throwaway probe: thirty
`Pine::Script::Manager::ReloadGameAssembly` calls in a row, resident set flat at 412 MB
(+628 kB over the whole run, and unchanged from the twelfth reload on) and the unload diagnostic
silent throughout. That was the starting point for unit 6, not a substitute for it — one script
class in one assembly is the easy case, and it is why unit 6's harder probe found a two-cycle
unload that the easy case never reached. `verify-script-reload.py` is that run, now repeatable.

What none of the probes reach is the *trigger*: the editor asks for a reload from a GLFW window
focus callback (`Editor/src/Utilities/Scripts/ScriptUtilities.cpp`), and no window manager runs
under Xvfb, so the focus event cannot be delivered from a script. The probes call
`Pine::Script::Manager::ReloadGameAssembly` directly, which is what that callback calls. Touching a
script, rebuilding and refocusing the editor is therefore still a thing to do by hand.

Additionally, from unit 3: a script with `[SerializeField] private float x`, `[HideInInspector]
public float y`, `[Range(0,10)] public float z`, a `public bool` and a **`public string`** should
render correctly and round-trip through save/load. Two of those five are there for a specific
reason: the bool for the width gotcha, and the string because it is the only field type whose value
payload is variable-length, so it is the only one that exercises both halves of the
length-then-fill crossing in 2.4. Neither `PlayerController.cs` nor the attribute set covers a
string otherwise, so it has to be added deliberately.

---

# Part 5 — Risks and open questions

| Risk | Severity | Mitigation |
|---|---|---|
| ALC fails to unload; editor leaks memory per reload | **High** — silent | Unit 6's `WeakReference` diagnostic; explicit handle teardown; move the `ScriptData`/`FieldRegistry` teardown ahead of `Unload()`, which means reordering `ReloadGameAssembly` (2.5) |
| SDK-style output lands in a `<tfm>/` subdirectory; nothing loads | Medium — the build succeeds and scripting is silently absent | `<AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>` on all four csprojs (2.7) |
| Managed reflection reflects inherited public fields; `Parent`/`Type` appear in the properties panel | Low — visible and harmless, but it ships | `BindingFlags.DeclaredOnly` plus the existing name skip (2.4) |
| `bool` width mismatch corrupts saved components | **High** — silent, and it corrupts data | Explicit 1-byte write; the round-trip check in Part 4 |
| `bool` bindings marshalled as 4-byte `BOOL` return garbage | **Medium** — compiles, misbehaves intermittently | The 24-site `byte` pass in 2.2, done as its own step in unit 5 |
| `Pine.dll` loaded into an isolated context, not Default; `Game.dll` cannot see it or gets a second copy | Medium — fails loudly at first load | Use `hdt_load_assembly` + `hdt_get_function_pointer`, not `hdt_load_assembly_and_get_function_pointer` (2.1) |
| Same second-copy failure via the build instead: CopyLocal puts `Pine.dll` in `runtime-bin/` and an `AssemblyDependencyResolver` prefers it | Medium — fails loudly, but looks like 2.1's bug and wastes the time spent re-checking 2.1 | `<Private>false</Private>` on the `Pine` reference; no resolver on the game ALC (2.5) |
| An exception escapes an `[UnmanagedCallersOnly]` entry point | **High** — kills the editor outright, with no managed stack trace | `try`/`catch` in every entry point as it is written, returning a failure value (2.2); `Type.GetType()` logs rather than throws (2.3) |
| Managed debugger attach on Linux is worse than Mono's | None — there is nothing to lose (see open question 2) | CoreCLR starts its diagnostics IPC server in any hosted process, so Rider/`vsdbg` attach as they would to any `dotnet` process; still **prototype before unit 5**, as an improvement rather than a regression check |
| GameHost deployment size | Low | Decide at unit 7 |

**Open questions to settle before starting:**

1. ~~**Which .NET version.**~~ Settled: `net10.0` (2.1). All four csprojs target it — `Pine.csproj`
   and the three `Game.csproj`, including `data/projects/project-template/` (2.7).
2. **Debugger attach.** An earlier draft framed this as a regression risk. On inspection there is
   nothing to regress: nothing in the tree calls `mono_debug_init`, nothing passes
   `--debugger-agent` through `mono_jit_parse_options`, and `MONO_ENV_OPTIONS` is not handled
   either, so managed debugging is not wired up today at all. That turns the question around.
   CoreCLR's attach path does not depend on who hosts the runtime, so the half-day spike before
   unit 5 is worth doing as *"get Rider stopping on a breakpoint in `Game.dll`"* — an outcome the
   migration can deliver and Mono currently does not.
3. **Windows.** The current code has never run there (`mono_config_parse("/etc/mono/config")` is
   hardcoded). Migrating removes that specific blocker, and the two Windows-specific choices that are
   cheap to make early are already in the design — no reliance on executable symbol export (2.2) and
   loading `Game.dll` from a stream so the file is not locked (2.5). Even so, do not treat "Windows
   works now" as an outcome of this plan without building it.

## Effort

Rough, for someone who knows this code:

| Unit | Estimate |
|---|---|
| 1 — header decoupling | 1–2 days (grew: it absorbs the `RenderScriptField` case conversion) |
| 2 — reflection-shaped bindings | 2 days |
| 3 — field reflection + attributes | 4–6 days (the Editor cases moved to unit 1; the value crossing moved in) |
| 4 — object factory | 3–4 days |
| 5 — runtime swap | 6–9 days (grew: the entry-point re-plumbing in 2.2 and the reload reorder in 2.5) |
| 6 — reload correctness | 2 days, **plus an unpredictable tail** |
| 7 — deployment and docs | 2 days |

Call it three to five focused weeks. Units 1–4 are predictable; unit 5 is large but mechanical; unit
6 is the one that can surprise you.

---

# Decisions

1. **Migrate to CoreCLR via `hostfxr`**, not to modern Mono's embedding API and not to NativeAOT.
2. **Do units 1–4 on Mono.** The runtime swap should be the *smallest* remaining step, not the first.
3. **Editor attributes land in unit 3**, before the swap, because managed-side field reflection is a
   prerequisite for both and is independently valuable.
4. **The C# game-facing API does not change.** Any proposal that would break
   `PlayerController.cs` is out of scope for this migration. This has already cost one design
   choice: `RayCastHit` stays non-blittable and gains a transport struct behind it, rather than
   having its `Entity` field reshaped (2.6).

   **One exception, taken deliberately in unit 5 and signed off by the author:**
   `Transform.Rotation` changed from `Vector3` to `Quaternion`. It was never a working property -
   the binding behind it writes a `Pine::Quaternion` through the pointer, so `out Vector3` was a
   16-byte write into a 12-byte slot. Keeping the old signature would have meant keeping a
   property that corrupts its caller's stack, and nothing in the tree calls it. Do not "restore"
   it.
5. **The saved format does not change.** `ScriptFieldType` values stay pinned; `ScriptFieldValue`
   keeps its byte layout; no `.passet` migration.
6. **`Pine.dll` in the default ALC, `Game.dll` collectible.** This is what makes reload simpler
   rather than merely different. A rebuilt `Pine.dll` therefore needs an editor restart; hot reload
   covers `Game.dll` only.
7. **Keep the C++ per-component dispatch loop.** Moving iteration into managed is a separate,
   later, measured change.
8. **C# → native goes through a function-pointer table, not `[LibraryImport]` into the host.**
   `Engine` is a static library, and symbol export from the executables would silently drop the
   binding translation units once their `Setup()` registrations are gone.

   **Refined in unit 5, with the author's agreement before the work started:** the table is
   resolved **by name** (`Script/Bindings/`) rather than being a struct whose members line up
   positionally on both sides. Same mechanism, same per-call cost; the difference is that a
   binding one side has and the other does not is a named error at startup instead of a silent
   misroute. See unit 5's "As built" notes. The same reasoning removed the entry-point struct
   2.1 step 4 describes, so **neither** direction has a hand-maintained parallel struct.
9. **Target `net10.0`.** Both SDKs are installed; .NET 8 leaves support in November 2026. All four
   csprojs are retargeted, `data/projects/project-template/runtime/Game.csproj` included.
10. **Nothing throws across the native boundary.** Every `[UnmanagedCallersOnly]` entry point
   catches, logs and returns a failure value; an escaping exception is a process kill, not an error
   the caller can handle.

---

Every departure from this plan that units 1-5 made is written up in the "As built" notes under the
unit that made it, and each one was put to the repository's author and accepted. They are recorded
rather than quietly folded in because the reasoning is the part that is expensive to reconstruct -
if a later change makes one of them wrong, the note says what it was trading off, which is what you
need to overturn it safely. Reversing one because it merely differs from the prose above is not an
improvement.

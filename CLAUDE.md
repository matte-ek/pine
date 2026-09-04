# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What Pine is

Pine is a 3D/2D game engine written in C++17, developed over several years as a personal project. The repo is one CMake tree producing an `Engine` static library plus three executables that link it: **Editor** (the GUI editor, uses Dear ImGui), **GameHost** (standalone game runner), and **EngineCli**. C# gameplay scripting is provided through a Mono runtime; the managed side lives in `ScriptRuntime/` and compiles to a `Pine.dll`.

## Working in this repo (git)

- **Commit only to the `ai` branch for now.** This branch is where AI-assisted work lands; do not commit to `main` (or other branches) unless explicitly asked.
- Do not commit prebuilt third-party binaries (see Dependencies below) unless the library's license clearly allows redistribution.

## Build & run

**Build (CMake only).** The author uses CLion, whose build dir is `cmake-build-debug/`. Equivalent from a shell:

```bash
cmake -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug -j            # all targets
cmake --build cmake-build-debug --target Editor -j   # single target
```

Targets: `Engine` (library), `Editor`, `GameHost`, `EngineCli`.

**Dependencies** are system packages plus bundled libs in `third-party/` (`imgui`, `material-icons`, `nvtt`, `perlin-noise`, `physx`). System packages on Arch: `glew assimp fmt mono openal glfw libjpeg-turbo libpng glm freetype2 nlohmann-json`. Note: the PhysX static libs in `third-party/physx/lib/*.a` and headers in `third-party/physx/include/` are **intentionally not committed** (kept out of version control unless a library's license clearly permits redistribution) — they must be present locally (`setup-env.sh` documents the PhysX build, though its copy paths predate the move into `third-party/`). Do not add prebuilt third-party binaries to git without checking the license first.

**Build the C# runtime.** From `ScriptRuntime/`, `msbuild -t:Build -p:Configuration=Release` (Release outputs `Pine.dll` to `data/engine/script/`, the path the engine loads). Targets .NET Framework `v4.7.2` via Mono. Each **project** has its own gameplay assembly under `data/projects/<name>/runtime/` (`Game.csproj`, globs `../assets/**/*.cs`, outputs `runtime-bin/Game.dll`); the Editor loads it per-project after selecting the project, GameHost loads its baked `data/game/runtime-bin/Game.dll`. Builds are external (IDE); the engine only watches/loads the DLL. See [`docs/scripting.md`](docs/scripting.md).

**Assets & shaders (`.passet` vs. source — read before touching either).** What the engine loads is the compiled **`.passet`**, built from raw source at import time — never hand-edit a `.passet`.
A **shader**'s source is its `.glsl` files plus an **`.ih`** ("import hint") JSON sidecar next to the `.passet` (lists the `SourceFiles` and sampler bindings). Two distinct workflows, and mixing them up costs time:
- **Editing an existing** shader/asset: change the `.glsl` and **run nothing**. Focusing the editor window triggers HotReload, which re-imports *and rewrites the `.passet` on disk* (`Asset::ReImport` → `ReLoad` → `File::WriteCompressed`), preserving the asset's UId. Commit that rewritten `.passet` — that is what makes GameHost and fresh clones (which don't hot-reload) correct. Gotcha: a shared `#include` (`shared/common.glsl`, `shared/lightning/*.glsl`) is **not** in the shader's `SourceFiles`, so editing one alone triggers nothing — also touch a top-level `.vertex.glsl`/`.fragment.glsl` of each shader that includes it.
- **⚠ Never run `EngineCli --import` on an asset that already has a `.passet`.** It builds a *new* asset: it mints a **new UId** — and assets reference each other by UId (`PINE_SERIALIZE_ASSET`, plus a hard-coded shader UId in `Material.hpp`), so this silently breaks every material pointing at that shader — and it never reads the `.ih`, dropping the shader `Versions` (`VERSION_DISCARD`, `VERSION_TERRAIN`), which cannot be declared in source. `--batch-import` keeps versions but still remints UIds (its dedup scan only reads top-level `data/*.passet`, never the nested engine shaders). If a committed `.passet` is stale, regenerate it by opening the editor, not from the CLI.
- **Adding a new** shader/asset: the engine only loads assets that already have a `.passet`; it will **not** auto-import unseen source. Generate it with **`EngineCli --import <engine-path> <source files…>`** run from `data/` (headless — no GL context, no engine boot). This is the *only* correct use of `--import`. Skipping it means `Assets::Get<T>()` returns null, and features that `assert` on their shader (e.g. `PostProcessing`, `AmbientOcclusion`, `Bloom`) will crash at boot — which also blocks the very import that would fix it. Commit the generated `.passet` + `.ih` alongside the source. Full details: [`docs/assets.md`](docs/assets.md).

**Tests:** there is no test suite or test framework in this repo. CI (`.github/workflows/cmake-build-linux.yml`) builds PhysX (cached) and compiles only the `Engine` target.

## Conventions

- **Subsystems are namespaces, not classes.** Nearly every subsystem is a `namespace Pine::<Name>` exposing free functions — a `Setup()`/`Shutdown()` (sometimes `Dispose()`) lifecycle plus `Update()`/`Run()` — with its mutable state in an anonymous `namespace { }` at the top of the `.cpp`. You reach a subsystem by calling `Pine::<Name>::Function()`, not by holding an object. Engine-internal entry points live in a nested `Internal` namespace (e.g. `Assets::Internal`, `WindowManager::Internal`). Editor code is under `namespace Editor`.

- **Layout:** one folder per class/subsystem, containing a `.hpp`/`.cpp` pair named after the folder (e.g. `World/Components/Camera/Camera.hpp`). Everything under `Engine/src/`, included as `Pine/...` (that's the include root). `CMakeLists.txt` uses `GLOB_RECURSE`, so a fresh CMake configure is needed after adding files.

- **Interfaces** are prefixed `I` and live in an `Interfaces/` folder — this is the backend seam (`Graphics/Interfaces/IGraphicsAPI` with an `OpenGL/` impl; `Audio/` mirrors this). **Handle** types (`EntityHandle`, `ComponentHandle<T>`, `AssetHandle<T>`, `ObjectHandle`) store an id + cached pointer and re-validate on access.

- **Logging:** `PInfo/PWarning/PError/PFatal/PVerbose(msg)` macros (fmt-formatted). Profiling: `PINE_PF_SCOPE()`.

- **Naming:** namespaces mirror directories; config/member fields commonly use an `m_` prefix.

## Architecture (the parts that span multiple files)

The summaries below are the map; **`docs/` holds a one-page orientation doc per subsystem** (key files, how the pieces fit, gotchas) — read the relevant one before working in that area. Index: [`docs/README.md`](docs/README.md).

- [`docs/world-ecs.md`](docs/world-ecs.md) — entities, components, the pool storage, the enum-ordering gotcha
- [`docs/assets.md`](docs/assets.md) — asset manager, the `.passet` format, serializer, Level/Blueprint
- [`docs/rendering.md`](docs/rendering.md) — Graphics wrapper, RenderManager, Renderer3D, pipelines & features
- [`docs/scripting.md`](docs/scripting.md) — the Mono/C# bridge and `ScriptRuntime/`
- [`docs/physics.md`](docs/physics.md) — Physics2D/Physics3D (PhysX) and the component coupling
- [`docs/core.md`](docs/core.md) — Core utilities (Math, File, Serialization, UId, Log, WindowManager)
- [`docs/editor.md`](docs/editor.md) — the Editor executable: boot, panels, viewports, play mode
- [`docs/data-and-projects.md`](docs/data-and-projects.md) — the `data/` tree, project structure, asset path resolution

- **Lifecycle** (`Pine::Engine`, driven by each app's `Application.cpp`): `Setup(EngineConfiguration)` → `Run()` → `Shutdown()`. Boot order matters: GLFW/window → Graphics → Audio → Threading → **Script runtime first** (so other systems can register C# bindings) → Assets (`LoadAssetsFromDirectory("engine")` must succeed) → Components/Entities → Rendering → Physics → Input → World → Game. `Run()` shows the window then loops: poll events → clear → `Input::Update()` + `World::Update()` → `RenderManager::Run()` → swap. Key `EngineConfiguration` flags: `m_MaxObjectCount` (caps entities **and** per-type components), `m_ProductionMode` (game vs. editor behavior), `m_Standalone`, `m_GraphicsAPI`.

- **ECS (custom, pool-based** — not EnTT/flecs). `World/`. Each component type gets a `ComponentDataBlock<T>`: a contiguous array of `m_MaxObjectCount` instances + a parallel occupation array, iterated by a custom iterator that skips empty/disabled slots. New components are placed in the first free slot and initialized by copying a default-constructed prototype. `Component` (base) defines the virtual lifecycle everything relies on: `OnCreated/OnDestroyed/OnCopied/OnSetup/OnUpdate/OnRender/OnPre|PostPhysicsUpdate` + `LoadData/SaveData`. `Entity` owns a `vector<Component*>`, a parent/child hierarchy, a `UId`, and always has a `Transform`. **Gotcha:** the `ComponentType` enum (`World/Components/Component/Component.hpp`) order must stay exactly in sync with the `CreateComponentDataBlock<T>()` call order in `Components::Setup()`. Adding a component = new `World/Components/<Name>/` folder **plus** edits to both of those.

- **Assets & the `.passet` format** (`Assets/`, `Pine::Assets` is the manager). A `.passet` file is the engine's own **compressed** binary container (`File::Read/WriteCompressed`): a header (UId, timestamp, `AssetType`, virtual path, source-file list) followed by a type-specific payload the subclass writes via `SaveAssetData()`/`LoadAssetData()`. `.passet` files are therefore not human-readable. Raw sources (`.png`, models, `.cs`) are brought in through the **Importer** pipeline (`Assets/Importer/`). Serialization is a reflective binary serializer (`Core/Serialization/`): declare a `struct XSerializer : Serialization::Serializer` and list fields with `PINE_SERIALIZE_PRIMITIVE/STRING/DATA/ARRAY/ASSET`; this same mechanism serializes assets, components, and scenes. A **scene** is a `Level` asset (skybox/ambient/fog/camera + a list of `Blueprint`s); a `Blueprint` is a serialized entity-with-components that `Spawn()` instantiates.

- **Rendering** — two layers. `Graphics/` is the API-agnostic GPU wrapper (`IGraphicsAPI` + `OpenGL/` impl: shader programs, buffers, textures, framebuffers, VAOs, UBOs). `Rendering/` is the engine renderer on top: `RenderManager::Run()` (once per frame) drives a list of `RenderingContext`s (each ≈ a camera/target — multiple can render simultaneously, e.g. editor viewport + game camera), running `Pipeline2D`/`Pipeline3D` with hookable stages (`AddRenderCallback`). `Renderer3D` is the low-level submission API (instanced batching, lights, shadows). `Rendering/Features/` holds pluggable passes (AmbientOcclusion, PostProcessing, Shadows, Skybox, TerrainRenderer); `Rendering/SceneProcessor/` walks the ECS blocks to gather draws/lights. Shaders/materials/meshes are all assets.

- **C# scripting (Mono)** — `Engine/src/Pine/Script/` on the C++ side, `ScriptRuntime/` on the managed side. `Script::Runtime` wraps Mono directly (`MonoDomain`, assembly load/unload, GC) and owns the engine's own `Pine` assembly. `Script::Manager` bridges gameplay: `LoadGameAssembly`, `ReloadScripts()` (hot reload), and per-frame `OnStart/OnUpdate/OnRender` dispatch, resolving Mono methods and reflecting serialized `ScriptField`s. `Script::ObjectFactory` gives every `Entity`/`Component`/`Asset` a paired managed object via an embedded `ObjectHandle`. `Script/Interfaces/` registers the native internal-calls (Log, Input, Entity, Component, Asset, Physics) that the C# side in `ScriptRuntime/` calls into. Two script component kinds exist: `ScriptComponent` (managed C#, backed by a `CSharpScript` asset) and `NativeScript` (C++, currently a stub).

## Author

I strive for performant solutions in the hot parts of the engine, rendering especially. I don't like solutions that work but are slow, sub-optimal, or paint us into a corner that needs a re-write to do something cooler later.

Pine will never be Unity, but I often ask myself "how do the big engines deal with this?" and work from there — they have the real-world experience about what holds up and what doesn't. Pine imitates the big *customizable* engines: it isn't locked to a genre or a game, and I don't want features that quietly assume one.

### What that means when you write code here

The failure mode I care about isn't a bad line of code — it's a locally-reasonable decision that the next feature then builds on top of. Each step looks fine in isolation; five steps later there's a pile of feature-specific code with no seam left to abstract. In domains I know well I catch this immediately; in engine-specific ones I can't always think that many steps ahead, so help me catch it:

- **Name and shape a mechanism after its concept, not its first caller.** If frustum culling only ever sees frustums from spot shadows today, it's still culling — not spot culling. The test: could a second, different kind of caller use it without renaming anything?
- **Don't build the second caller — flag it.** Say in your response what the next plausible user of the mechanism would need, and whether the current shape can take it. Deciding whether to build it now is my call; the cost of telling me is one sentence, and it beats finding out five features later.
- **When you extend something, say what you're building on top of.** "This works, but only because X still assumes Y" is what I want to hear before the change lands, not after. Silently inheriting an assumption is how the pile grows.
- **Generality has a real cost — don't pay it everywhere.** Leaf code (editor panels, one-off game logic, a single render feature's internals) should stay concrete. The above applies to engine mechanisms that other features will sit on top of.

### Other

* Please feel free to both challenge my ideas or proposals, if you think what I am saying is genuinely a bad idea, let me know. Iterating together gives the best solutions.
* Please feel free to point out issues with existing code. Existing code can be changed and improved, and in a lot of cases, especially when implementing a new feature, it might be very beneficial.
* Please feel free to ask if anything is unclear.

## How we work

- **Plan before implementing anything that adds or reshapes a mechanism.** Same line as above: if another feature will sit on top of it, plan first — the shape is the part worth arguing about, and text is far cheaper to iterate on than build-run cycles (no test suite here, and I'm the one running the editor). Leaf code, bug fixes and shader tweaks: just do it.
- **Reports for read-only questions.** "What's the current state of X", "how would we implement Y", "what's this going to cost us" — write it up rather than answering inline and losing it. A report is for a decision not being made yet; a plan is for one being made now.
- **Reports are transient.** Publish as a markdown files, into `docs/reports`.
- **A report earns its length in analysis, not inventory.** Don't restate code I wrote. Tell me what the current shape can't take, what the next plausible caller would need, and which existing assumptions a proposal would inherit.

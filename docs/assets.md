# Asset system & the `.passet` format

How Pine loads, stores and serializes content. Paths relative to `Engine/src/Pine/`.

## Start here
- `Assets/Assets.hpp` — the manager (`Pine::Assets`): `LoadAssetFromFile`, `LoadAssetsFromDirectory`, `CreateAsset`, `Get<T>(uid|path)`, `GetAll()`, `SetWorkingDirectory`.
- `Assets/Asset/Asset.{hpp,cpp}` — the abstract `Asset` base + the `.passet` load/save envelope.
- `Assets/Importer/` — the import pipeline that turns raw source files into assets.
- `Core/Serialization/Serialization.hpp` — the reflective binary serializer used everywhere.

## The `.passet` format
A `.passet` file is the engine's **own compressed binary container** (`Core/File` →
`File::Read/WriteCompressed`) — not human-readable. Layout:
- a **header** (UId, timestamp, `AssetType`, virtual path, source-file list), then
- an opaque **payload** the concrete asset writes via `SaveAssetData()` / reads via `LoadAssetData()`.

`Asset::Load()` reads the header, instantiates the right subclass by `AssetType`, dedups by
UId+time, then hands the payload to the subclass. Raw sources (`.png`, models, `.cs`) are
*imported* into `.passet` through `Assets/Importer/` (with per-type `Importer/` subfolders
under `Texture2D/`, `Model/`, `Shader/`).

Not every asset type has a source format. A `Terrain` is authored in the editor and only ever
exists as a `.passet`, so its row in `m_AssetImportFactories` (`Assets.cpp`) carries no file
extensions. The row itself still has to be there: that list is also the `AssetType` -> constructor
lookup `Asset::Load()` and `Assets::CreateAsset()` both go through, so deleting it would stop the
type from loading at all.

### The import phases

`Assets/Importer/AssetImporter.hpp` splits an import into phases, and a caller can stop between
them:

- **`Resolve()`** decides, for every queued file, what it is (`AssetType`), where it lands
  (`ResolvedEnginePath`) and what will happen to it (`AssetImportAction`: `Create`, `Update`,
  `Unsupported` or `Conflict`). It constructs the asset object but writes nothing to disk, so the
  plan can be shown or thrown away. **`Update` is how importing over an existing asset keeps its
  UId** — the asset already loaded at the destination is re-imported in place instead of a new one
  being created over it.
- **`ProposeImportSettings()`** lets each resolved asset work out import settings from its source
  files (`Asset::ResolveImportSettings()`). Split from `Resolve()` because the two differ in what
  they touch: resolving only decides, whereas this *changes an asset's settings* — and for an
  `Update` entry that asset is a live, loaded one, so a caller that means to show the plan first
  gets to snapshot before it happens.
- **`Execute()`** compiles (`Asset::Import()`) and commits (`ReLoad()` + registration) everything
  `Resolve()` marked `Create` or `Update`, proposing settings first if nobody has.
  **`ExecuteNext()`** is the same work one entry at a time, advancing `ImportContext::ExecuteCursor`
  — that is what lets the editor keep painting and draw progress during a large import. Pacing is
  the caller's: the importer knows nothing about frames.

`Run()` is all of it, for callers that don't care about the plan. Because the asset exists from
`Resolve()` onwards, per-asset import settings (e.g. `Texture2D::GetImportConfiguration()`) can be
read and changed *before* anything compiles; that is how `ModelImporter` tells a texture it is a
normal map, and it is what the editor's import dialog
(`Editor/src/Gui/Dialogs/AssetImport/`) edits.

The editor utility `Utilities::Asset::CreateImportContext()` sets up source copying into the
project's `content/` folder. Its explicit destination-directory overload is also used by
[`POST /assets/import`](debug-server-import.md), which executes synchronously and returns the
registered asset IDs. Copying a source already at its content destination preserves that file.

**Progress counters must not use `Imports.size()`.** Importing a model appends the textures it
discovers to the queue, so the list grows as the import runs; count the entries that were there
when the plan was shown instead, or "3/58" becomes "3/2400" halfway through a folder of models.

### Where a texture's usage hint comes from

`TextureUsageHint` decides a texture's block compression format *and* whether the GPU sRGB-decodes
it, so guessing it wrong is not cosmetic. Several things have an opinion, and they are ranked
(`TextureUsageHintSource`, serialized alongside the hint):

    User  >  SourceFormat  >  Heuristic  >  Default

Everything goes through `ApplyTextureUsageHint()`, which only writes when the new reason is at
least as good as the recorded one. That is what makes re-import safe: `ModelImporter` reading an
`aiTextureType` slot (`SourceFormat`) and `GuessTextureUsageHint()` reading the file name
(`Heuristic`) can both run on every re-import without touching a hint set by hand.

The heuristic reads the **trailing token** of the file name (`blood_1_normal`), then the immediate
parent directory (`PSX Textures/Normal Maps/`). Only the trailing token, and only unambiguous
tokens: `gm`'s pack has `metal_floor_5` and `floor_3_metal`, both albedo textures of metal, which
is why `metal` and `rough` are deliberately *not* in the table.

## Editing shaders: `.passet` vs. raw GLSL and the `.ih` hint
Shaders follow the same rule as every other asset: the `.passet` is a **compiled binary
container built from raw source at (re)load time**, so you never hand-edit it. For a shader,
the source lives in `.glsl` files and the `.ih` ("import hint") file next to the `.passet`
points at them. **To read or change shader code, open the `.glsl` the `.ih` references — not
the `.passet`.**

An `.ih` is small JSON, e.g. `data/engine/shaders/post-processing/ambient-occlusion.ih`:
- **`SourceFiles`** — the raw GLSL stages (vertex/fragment) this shader is built from.
- **`Data.TextureSamplers`** — sampler name → binding unit (mirrors the `#shader bind <name> <unit>`
  directives at the top of the GLSL).
- **`Data.Versions`** (optional) — preprocessor `#define` variants (e.g. `VERSION_DISCARD`) the
  shader can be compiled with. **Only `EngineCli --batch-import` reads this**, and that reminting
  every UId it touches (below) makes it unusable on an existing shader — so declare a *new* version
  in the GLSL instead, with `#shader version <NAME> <bit>` next to the `#ifdef` it guards
  (`terrain.fragment.glsl` does). That goes through the ordinary importer, so a re-import picks it
  up and writes it into the `.passet` like any other source change. Any other `#shader <anything>`
  line is stripped, and the importer warns that it did.

  A version that is declared nowhere still *compiles*: `CompileShaderVersion` builds its `#define`
  list from the registered entries, so an unregistered bit yields a second program built from
  unchanged source. The `#ifdef` body is then silently absent, which is why it is worth asserting
  that a variant's own uniforms exist rather than assuming the variant did anything —
  `verify-terrain-sculpt.py` does exactly that for `VERSION_BRUSH`.

The same source→`.passet` relationship holds for other imported assets (textures, models);
shaders just expose it as editable text with a sidecar hint.

### Regenerating a `.passet` after editing source

**Just open the editor.** On window focus, `HotReload::UpdateAssets` compares each tracked
source's write time and calls `Asset::ReImport()` → `ReLoad()` → `File::WriteCompressed(...)`
(`Asset.cpp`). That rewrites the `.passet` **on disk**, operating on the already-loaded asset so
the **UId is preserved**. Commit the rewritten `.passet`; that is what makes GameHost and fresh
clones (neither of which hot-reloads) correct. There is no CLI step for editing an existing asset.

Two things that bite:

- **Shared `#include`s are not tracked sources.** The importer inlines `#include`d files
  (recursively, through the same line processor — so `#shader bind` directives inside an include
  *are* picked up), but it never adds them to the shader's `SourceFiles`. Editing
  `shared/common.glsl` or `shared/lightning/*.glsl` alone therefore triggers no reload. Touch a
  top-level `.vertex.glsl`/`.fragment.glsl` of every shader that includes it.
- **Never `EngineCli --import` an asset that already has a `.passet`.** It constructs a *new*
  asset, so it would mint a **new UId** — and assets reference each other by UId
  (`PINE_SERIALIZE_ASSET`, plus a hard-coded shader UId in `Material.hpp`), so every material
  pointing at that shader would silently break. The importer now refuses this outright
  (`AssetImportAction::Conflict`) rather than doing it quietly, so you get an error instead of a
  corrupted project — but it still means there is no CLI path for re-importing. It also never
  reads the `.ih`, so `Data.Versions` is lost. `--batch-import` does read the `.ih`, but its "already imported" dedup scan is a
  non-recursive `directory_iterator("data")`, so it never finds the nested engine shaders and
  remints their UIds too. `--import` is for **first-time** imports of new assets only. This is the
  reason a new shader version belongs in the GLSL rather than in the `.ih`.
- **Run `--import` from inside `data/`, with paths relative to it.** The virtual path is the
  engine path with the asset working directory stripped off, and headless there is no working
  directory to strip — so `EngineCli --import data/engine/shaders/3d/x ...` from the repo root
  gives the asset the path `data/engine/shaders/3d/x`, which is not where anything looks for it.
- **Without a window there is still a way to re-import.** `Asset::ReImport()` does the whole job
  in process, so a throwaway probe built the way
  `Editor/src/DebugServer/Verification/verify-*.py` build theirs — same compile and link commands,
  with the `Engine::Run()` call replaced — can regenerate a `.passet` against a copy of `data/` and
  have the result copied back. That is worth knowing when a shared `#include` changed and the
  editor is not to hand.

**Scripts are the exception.** A `CSharpScript` `.passet` is *not* built from its `.cs` — the
`.cs` compiles into the project's `Game.dll` separately, and the `.passet` just stores the
managed type name + registers the `.cs` as an `AssetSource` sitting next to it. So there is no
`.ih` for scripts and the `.passet` is authored, not regenerated. See [scripting.md](scripting.md).

## Asset types
One folder per type under `Assets/`, each subclassing `Asset`: `Blueprint`, `Level`,
`Material`, `Mesh`, `Model`, `Shader`, `Texture2D`, `Texture3D`, `Font`, `Tileset`,
`Tilemap`, `AudioFile`, `CSharpScript`, `Terrain` (+ `InvalidAsset`).

## Scenes: Level & Blueprint
- **`Level`** (`Assets/Level/`) is the scene: settings (skybox `Texture3D`, ambient/fog, camera entity) + a list of `Blueprint`s. `World::SetActiveLevel(Level*)` loads it.
- **`Blueprint`** (`Assets/Blueprint/`) is a serialized entity + its components. `Spawn()` instantiates it into the world; `CreateFromEntity()` captures one.
- Blueprint copying preserves entity tags and component active flags. Component
  active flags are serialized; older files without the field default to active.

## Serialization
Declare a `struct XSerializer : Serialization::Serializer` and list fields with macros:
`PINE_SERIALIZE_PRIMITIVE(name, DataType)`, `PINE_SERIALIZE_STRING`, `PINE_SERIALIZE_DATA`,
`PINE_SERIALIZE_ARRAY`, `PINE_SERIALIZE_ASSET` (stores an asset by its UId). The **same**
mechanism serializes assets, component `LoadData/SaveData`, and scenes. A JSON variant lives
in `Core/Serialization/Json/`.

Related: [world-ecs.md](world-ecs.md) · [rendering.md](rendering.md) (materials/shaders/meshes are assets)

### Level camera identity

Newly captured Levels store the selected game camera as a one-based index in
serialized root/descendant order, marked by `CameraUsesSerializedOrder`. Zero clears
the camera. This survives reparenting and differences in editor-only entity counts.
Files without the flag are read as the legacy live-list index, which counted the editor's own
temporary entity ahead of the scene; re-saving migrates them.
See [scene camera persistence](debug-server-scene-camera.md#observe-and-persist).

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

`Asset::Load()` reads the header and looks the UId up first: an asset already loaded with the same
timestamp is returned as is, and one with a different timestamp is reloaded in place. Only an
unknown UId gets a new subclass instance by `AssetType`. The payload then goes to the subclass.
Raw sources (`.png`, models, `.cs`) are *imported* into `.passet` through `Assets/Importer/` (with
per-type `Importer/` subfolders under `Texture2D/`, `Model/`, `Shader/`, `AudioFile/`).

Not every asset type has a source format. A `Terrain` is authored in the editor and only ever
exists as a `.passet`, so its row in `m_AssetImportFactories` (`Assets.cpp`) carries no file
extensions. The row itself still has to be there: that list is also the `AssetType` -> constructor
lookup `Asset::Load()` and `Assets::CreateAsset()` both go through, so deleting it would stop the
type from loading at all.

### Saving an asset that was not just imported

An asset type whose payload is bulk data - a texture's pixels, a model's geometry, an audio clip's
samples - keeps no copy of it in system memory once loaded, because it lives on the GPU or the
audio device. So `SaveAssetData()` for those types has nothing to write when the save is not
straight after an import, and has to recover the payload from the `.passet` it is replacing.
**Read it back with `Asset::ReadStoredAssetData()`, never by reading the file yourself.**

A `.passet` is the `AssetSerializer` envelope (`UId`/`Time`/`Type`/`Path`/`Sources`/`Data`) with
the type's own payload inside its `Data` field. Handing the whole file to the payload serializer
*succeeds* and populates nothing, because `Serialization::Serializer::Read` matches fields by name
and none of the payload's names appear in the envelope. The asset then saves with its bulk data
dropped, `ReLoad()` writes that over the only good copy, and nothing about the asset in memory
looks wrong at the time. `Texture2D` and `Model` both did exactly this; `verify-asset-resave.py`
is what keeps them from doing it again.

That read fills **every** field of the payload serializer, not only the bulk data, and reading an
array appends to it. Anything the same save then writes from memory has to be reset first, or it
is stored twice. `Model::SaveAssetData` resets `EmbeddedMaterials` and `LodLevels` for that reason.
The stored meshes are the opposite case: their geometry is carried forward as read, but each one's
`Material` is written again from `Mesh::GetMaterialUId()`, because the editor can point a mesh at
another material after loading.

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
[`POST /assets/import`](debug-server.md#post-assetsimport), which executes synchronously and returns the
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

### What a texture does with its alpha

`TextureImporter::Import` scans the **source image's** alpha channel before anything is block
compressed, and records the answer on the texture as a `TextureAlphaMode`:

| Mode | Meaning |
|------|---------|
| `Opaque` | Every pixel is solid, or there is no alpha channel. |
| `Cutout` | Alpha is a mask - a pixel is either there or it is not. Antialiased edges count as this. |
| `Transparent` | Enough of the image is partially see-through, *and* that partial alpha sits inside the shape rather than on its outline: glass, water, smoke. |
| `Unknown` | Never scanned. The texture was imported before this existed, and nothing is derived from it until it is re-imported. |

`Cutout` and `Transparent` are told apart by two tests, in that order. The first is the cheap one:
under 10% partial-alpha pixels and the texture is a mask whatever its shape. The second decides the
rest, and it asks how that partial alpha is *laid out* rather than how much of it there is. A
partial pixel bordering a solid or a fully clear one is on an antialiased outline; one with nothing
but partial pixels around it sits inside a region that genuinely fades. Over half of them have to be
interior for `Transparent`.

Counting partial pixels alone does not work, because foliage is all perimeter - at 256px a grass
blade's antialiased edge is a large share of the image. Measured over `gm`'s PSX Nature pack, the
outlines of grass, ferns and pine branches cover 8-28% of their textures, while under 30% of those
pixels are interior, against over 95% for a genuinely translucent one. Getting it wrong in that
direction is the expensive mistake: `ShadowPass` builds draw lists for `Opaque` and `Discard` only,
so calling foliage `Transparent` silently costs it its shadow.

Two things read the result:

- **`Material::ResolveRenderingModeFromDiffuse()`** maps it onto `MaterialRenderingMode`
  (Opaque / Discard / Transparent). Anything that hands a material a new diffuse map calls it: the
  model importer, so a model's own materials land in the right pass without being touched, and the
  editor's diffuse picker. `Unknown` leaves the material's mode alone.
- **The compression format.** BC1 holds no alpha and BC1a holds one bit of it, so a texture that
  genuinely fades is moved off the fast `AlbedoFaster` default onto `Albedo` (BC7). That goes
  through `ApplyTextureUsageHint` at the `Heuristic` tier, so it can override the default and the
  file-name guess but never a hint the model file supplied or the user picked. Note the PNG loader
  returns RGBA for *every* image, filler alpha included - which is why BC1 vs BC1a is chosen from
  the alpha mode rather than from the channel count.

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
  shader can be compiled with. **Only `EngineCli --batch-import` reads this**, and it refuses a
  shader that already has a `.passet` (below), so it is no use on an existing one — declare a *new* version
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

**Just open the editor.** On window focus, `Utilities/HotReload/HotReload.cpp` compares each
tracked source's write time and calls `Asset::ReImport()` → `ReLoad()` → `File::WriteCompressed(...)`
(`Asset.cpp`). That rewrites the `.passet` **on disk**, operating on the already-loaded asset so
the **UId is preserved**. Commit the rewritten `.passet`; that is what makes fresh clones correct.
There is no CLI step for editing an existing asset.

Hot reload is set up by `Engine::Setup` whenever `m_EnableDebugTools` is on (the default), so
GameHost does it too. The tracked set is a snapshot `Utilities::HotReload::ReloadCache` takes once,
at the end of `Engine::Setup`: that is the `engine/` assets only. The editor's own assets and a
project's, loaded afterwards, are not hot-reloaded.

Things that bite:

- **Shared `#include`s are not tracked sources.** The importer inlines `#include`d files
  (recursively, through the same line processor — so `#shader bind` directives inside an include
  *are* picked up), but it never adds them to the shader's `SourceFiles`. Editing
  `data/engine/shaders/3d/shared/common.glsl` or `3d/shared/lightning/*.glsl` alone therefore
  triggers no reload. Touch a
  top-level `.vertex.glsl`/`.fragment.glsl` of every shader that includes it.
- **Never `EngineCli --import` an asset that already has a `.passet`.** It constructs a *new*
  asset, so it would mint a **new UId** — and assets reference each other by UId
  (`PINE_SERIALIZE_ASSET`, plus a hard-coded shader UId in `Material.hpp`), so every material
  pointing at that shader would silently break. The importer refuses this outright
  (`AssetImportAction::Conflict` from `Importer::Resolve`), so you get an error instead of a
  corrupted project — but it means there is no CLI path for re-importing. `--import` also never
  reads the `.ih`, so `Data.Versions` is lost. `--batch-import` does read the `.ih`, but it imports
  through the same `Importer::Run`, so an existing shader is refused there too. (Its own "already
  imported" scan is a non-recursive `directory_iterator("data")` and never finds the nested engine
  shaders; with a map-root, whose resolved path misses the existing `.passet`, it would still mint a
  new UId.) `--import` is for **first-time** imports of new assets only. This is the reason a new
  shader version belongs in the GLSL rather than in the `.ih`.
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

**Two names per type, and they do different jobs.** `AssetTypeToString` is the identifier: the
`type` token the debug server's `/catalog` accepts and reports, so changing one of those strings is
a contract change. `AssetTypeToHumanString` is the label the editor puts in front of a person, and
is free to read better - `CSharpScript` is shown as "Script", `Tilemap` as "Tile-map". Both live in
`Assets/Asset/Asset.hpp`.

C# never sees either string. `Script::ObjectFactory::CreateAsset` passes the `AssetType` as an
integer, and the managed side names it through its own `Pine.Assets.AssetType` enum
(`ScriptRuntime/Assets/Asset.cs`): that name is the class `ObjectFactory.cs` creates and the one
`FieldRegistry.cs` matches a script field's type against. So the C++ `AssetType` enum's order has
to match the managed enum, and each managed enum name has to match its class in `ScriptRuntime/Assets/`.

### Audio: what an `AudioFile` stores

An `AudioFile` stores its clip in one of two encodings, recorded in the payload's `Encoding` field
(`AudioEncoding`, in `Assets/AudioFile/AudioFile.hpp`):

- **Ogg Vorbis sources keep their own bytes.** The importer (`Assets/AudioFile/Importer/`) copies
  the `.ogg` into the `.passet` unchanged and `AudioFile::LoadAssetData` decodes it with
  `stb_vorbis` on every load, on the asset-loading worker thread. It is still decoded at import
  too, so a file that does not decode fails its import, and so the format, sample rate and
  per-channel sample count stored alongside it are real.
- **Wave sources are stored as PCM**: decoded at import into interleaved signed 16-bit samples, and
  uploaded as they are on load. There is no Vorbis *encoder* in the tree, so a wave clip cannot be
  stored any smaller.

Either way, the whole clip is decoded by the time it has loaded, and lives in one
`Audio::IAudioBuffer`. A clip stored before `Encoding` existed has no such field and loads as PCM,
which is what it is.

- **Decoders** live in `Importer/AudioLoader/Formats/`, picked by file extension, and all produce
  the same `AudioLoader::AudioData`. Wave covers 8/16/24/32-bit PCM and 32/64-bit float, including
  `WAVE_FORMAT_EXTENSIBLE`; Ogg Vorbis goes through `stb_vorbis`. `.oga` is read as Vorbis and
  refused if it holds anything else. FLAC and Speex are not supported, and are deliberately **not**
  in the factory's extension list - claiming an extension and then failing the import is worse than
  not claiming it. `AudioLoader::DecodeVorbis` and `AudioLoader::DownmixToMono` are also what
  `AudioFile` uses at load.
- **`ForceMono`** is the only import setting. OpenAL pans and attenuates *mono* buffers only, so a
  stereo clip on a positioned `AudioSource` plays flat wherever its entity is; this is how a stereo
  source file is made usable for 3D sound. It is off by default, because it is the wrong thing to
  do to music. Anything above two channels has no format to be stored as and is folded down whether
  or not it was asked for. The import settles the clip's stored `Format`, and that is what takes
  effect, so ticking Force Mono needs a re-import. A PCM clip is folded at import; a Vorbis clip
  keeps its source's stereo bytes and is folded to its `Format` on every load.
- **Size.** A Vorbis clip costs about what its `.ogg` does on disk. In memory every clip costs its
  decoded size, roughly `sampleCount * channels * 2` bytes (`AudioFile::GetSampleDataSize`): about
  30 MB for a three minute stereo track. Streaming long clips is what would change that; see
  `Audio/TODO.md`.

`Engine/src/Pine/Audio/` is the other half: `IAudioAPI` (OpenAL) creates the `IAudioBuffer` a clip
uploads into, and the voice pool and the `AudioSource`/`AudioListener` components play it. See
[audio.md](audio.md).

### Model LOD settings

A `Model` carries its own level-of-detail chain: `Model::GetLodLevels()`, each a handle to another
`Model` asset plus the distance it takes over from, and `Model::GetLodCullDistance()`, past which
the object is not drawn (0 means never). They are set on the model's page in the Editor's asset
properties, and saved in the `LodLevels` and `LodCullDistance` fields of the payload. A model
stored before those fields existed loads with no levels.

- **One model per level**, referenced by `UId`. A pack that ships `rock_LOD0.fbx`, `rock_LOD1.fbx`
  imports each file as its own model and lists the others on `rock_LOD0`. An importer that splits
  one file's `_LOD0`/`_LOD1` meshes into models would fill the same list.
- **Only the list on the model a renderer names is read.** A model used as a level is drawn as it
  is, whatever levels it has of its own.
- **Not kept sorted.** `Model::SelectLod` picks the furthest level reached, so the Editor can show
  the levels in the order they were entered. A level whose model is missing is skipped.
- **The importer leaves them alone**, so they survive a re-import of the source file.
- **Materials are per file.** Each level's model embeds its own materials, so editing LOD0's does
  not change LOD1's, and the levels never batch with each other. A `ModelRenderer` override
  material applies to whichever level is drawn.

How a level is chosen at draw time is in [rendering.md](rendering.md#model-lod).

### A model's embedded materials

The materials a model file defines are not assets of their own. Each one is stored inside the
model's payload (`EmbeddedMaterials`), has no `.passet` and no file path, and is registered under
the virtual path `<model path>-<material name>` by `Model::LoadAssetData`, which also records the
model on it. `Material::IsEmbedded()` and `Material::GetEmbeddingModel()` read that back.

- **Editing one saves the model.** The editor lets you edit an embedded material like any other,
  but marks its *model* as modified, because the model's `.passet` is what has to be rewritten.
  `Model::SaveAssetData` writes every embedded material from memory. Saving all assets skips
  anything without a file path, so marking the material itself would save nothing.
- **Re-importing keeps what was set in the editor, except what the file defines.** The importer
  finds each material again by its path and updates it in place, so its UId survives. It sets the
  colors, the shininess and the three texture maps from the file again. It re-derives the rendering
  mode only when the file now names a different diffuse map, and it only ever sets the render face
  to `Both` (for a file that says the material is two-sided), never back to `Default`. The fields
  a model file has no say in (specular color, shader, texture scale, alpha) are left alone.
- A material that disappears from the file on re-import stays registered until the next start,
  but the model no longer stores it.

### Reading geometry back out

A `Mesh` keeps no CPU copy of what it uploaded, so
`Mesh::ReadGeometry` asks the graphics API for it — positions, plus indices if the mesh has an
element buffer. That means it needs the graphics context and stalls until the readback lands, which
is fine for tooling and not for a frame. What comes back is current, including whatever
`UpdateVertices` last wrote, so a procedurally rebuilt mesh reads back the geometry it is actually
drawing. The debug server's stopped-mode raycasts are the caller, and they cap how much geometry
they read per request.

`Terrain` needs none of that. `Terrain::Raycast` walks the live height field rather than the
rendered chunks, so what it returns — the contact point, the distance along the ray, and the
upward face normal of the triangle it met — does not depend on which LOD a chunk is drawn at.

## Scenes: Level & Blueprint
- **`Level`** (`Assets/Level/`) is the scene: settings (skybox `Texture3D`, ambient/fog, camera entity) + a list of `Blueprint`s. `World::SetActiveLevel(Level*)` loads it.
- **`Blueprint`** (`Assets/Blueprint/`) is a serialized entity + its components. `Spawn()` instantiates it into the world; `CreateFromEntity()` captures one.
- Blueprint copying preserves entity tags and component active flags. Component
  active flags are serialized; older files without the field default to active.

## Serialization
Declare a `struct XSerializer : Serialization::Serializer` and list fields with macros:
`PINE_SERIALIZE_PRIMITIVE(name, DataType)`, `PINE_SERIALIZE_STRING`, `PINE_SERIALIZE_DATA`,
`PINE_SERIALIZE_ARRAY`, `PINE_SERIALIZE_ARRAY_FIXED(name, type)` (fixed-size elements, e.g.
`AudioFile` samples), `PINE_SERIALIZE_ASSET` (stores an asset by its UId). The **same**
mechanism serializes assets, component `LoadData/SaveData`, and scenes. A JSON variant lives
in `Core/Serialization/Json/`, and `Core/Serialization/Dump/` turns any serialized blob into
readable JSON - that is what `EngineCli --dump <file>` prints.

Related: [world-ecs.md](world-ecs.md) · [rendering.md](rendering.md) (materials/shaders/meshes are assets)

### Level camera identity

Newly captured Levels store the selected game camera as a one-based index in
serialized root/descendant order, marked by `CameraUsesSerializedOrder`. Zero clears
the camera. This survives reparenting and differences in editor-only entity counts.
Files without the flag are read as the legacy live-list index, which counted the editor's own
temporary entity ahead of the scene; re-saving migrates them.
See [game camera selection](debug-server.md#get-levelcamera-post-levelcamera).

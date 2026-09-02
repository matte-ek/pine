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
- **`Data.Versions`** (optional) — preprocessor `#define` variants (e.g. `VERSION_TERRAIN`) the
  shader can be compiled with.

Edit the `.glsl` and the engine rebuilds the `.passet` on load. The same source→`.passet`
relationship holds for other imported assets (textures, models); shaders just expose it as
editable text with a sidecar hint.

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

## Serialization
Declare a `struct XSerializer : Serialization::Serializer` and list fields with macros:
`PINE_SERIALIZE_PRIMITIVE(name, DataType)`, `PINE_SERIALIZE_STRING`, `PINE_SERIALIZE_DATA`,
`PINE_SERIALIZE_ARRAY`, `PINE_SERIALIZE_ASSET` (stores an asset by its UId). The **same**
mechanism serializes assets, component `LoadData/SaveData`, and scenes. A JSON variant lives
in `Core/Serialization/Json/`.

Related: [world-ecs.md](world-ecs.md) · [rendering.md](rendering.md) (materials/shaders/meshes are assets)

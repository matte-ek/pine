# Report: a review step, per-asset import settings, progress, and a smarter importer

Scope: what it would take to (1) show the user what an import will produce *before* it runs,
(2) let them change import settings per file/directory, (3) show progress while it runs, and
(4) make the importer infer things like "this is a normal map". Paths relative to repo root.

## Verdict up front

The pieces exist, but not in the arrangement these features need. The blocker is not missing
infrastructure — it's that **`Importer::Run()` fuses three separate phases** that all three
features need to be able to stand between:

| Phase | What it decides | Where it lives today |
|---|---|---|
| **Resolve** | what type each file is, where it lands, whether it collides with an existing asset | inline inside `Import()` (`AssetImporter.cpp:24`), after the point of no return |
| **Compile** | run `Asset::Import()` — nvtt compression, assimp parse. Pure CPU, no GL | same function |
| **Commit** | `ReLoad()` + `RegisterAsset()` — writes the `.passet`, uploads to GL, mutates the global asset maps | same function |

A review list is "stop after Resolve". Per-asset settings is "let the user edit between Resolve
and Compile". A progress bar is "run Compile off the main thread and Commit on it". They are the
same refactor viewed from three angles, which is good news: build the seam once and all three
become UI work.

Three bugs fall out of the same reading, and two of them are live today (§5).

## 1. Reviewing before committing

Everything a review list wants to display is *derivable* — it's just derived too late.

- **Type** comes from `GetAssetFactoryFromFileName` (`Assets.cpp:68`), a pure extension lookup.
- **Destination path** comes from `SetupNew` + the `AvoidDuplicate` suffix logic.
- **Already-imported?** is answerable from `Assets::GetAll()` + `Asset::GetSources()`. EngineCli's
  `--batch-import` already builds exactly this map (source path → asset) and uses it to re-import
  in place instead of minting a new asset. **The editor does not.** Today, re-dropping a folder you
  already imported calls `SetupNew` again → new UId written over the same `.passet` → every material
  referencing the old UId breaks on next launch. This is the single strongest argument for the
  review step: it isn't a nicety, it's where "Update existing (3 assets)" vs "Create new" gets
  decided, and it's currently decided wrong and silently.
- **Failure modes** are knowable in advance for the cases that matter: no factory for the extension,
  and — worth surfacing — the types whose `Import()` is the base-class no-op (`Asset.cpp:307`).
  Only `Texture2D`, `Shader` and `Model` override it. `Font` and `AudioFile` read straight from
  `m_FilePath`, which after `SetupNew` points at the `.passet`, so dropping a `.ttf` or `.wav`
  produces an asset that cannot work. No `.ttf`/`.wav` `.passet` exists anywhere in `data/`, so this
  path has evidently never been exercised. A review list makes that visible instead of mysterious.

**Shape.** Split `Run()` into `Importer::Resolve(context)` and `Importer::Execute(context)`, and
widen `AssetImport` with the resolved fields (`Type`, final `EnginePath`, `ExistingAsset*`,
`Action` ∈ {Create, Update, Skip, Unsupported}). `Resolve` is cheap — extension lookup plus a map
probe — so it can run synchronously in the drop handler for a 2500-file pack.

The thing to *not* do is compute this a second time in the editor for display purposes. The list
must show the same values the importer will act on, or it becomes a lie the first time the two
drift.

## 2. Per-asset / per-directory import settings

### The plumbing is already stubbed, and the stub is dead

`AssetImport::Configuration` (`AssetImporter.hpp:40`) is an `AssetImportConfiguration*` that is
stored, passed around — and **never read by anything**. `Texture2D::Import` ignores its `context`
argument entirely and uses `m_ImportConfiguration`, the copy that lives on the asset and is
serialized into the `.passet` (`Texture2D.cpp:36,285`).

Asset-owned, asset-persisted config is the *right* answer (it's what Unity's `.meta` is), so the
fix is not to make the context pointer work — it's to delete it and let the caller reach the
config through the asset. Which requires the asset object to exist before Compile. Which the
Resolve phase from §1 gives you for free: `CreateAssetByFile` already constructs the (empty) asset
object as the first thing `Import()` does, and doing it in Resolve instead costs nothing.

That collapses the design nicely:

```
Resolve  → each AssetImport owns a constructed Asset* with default config
(user edits asset->GetImportConfiguration() in the review list — the same struct,
 through the same widgets, that AssetPropertiesRenderer.cpp:69-71 already draws)
Compile  → Asset::Import()
Commit   → ReLoad() + RegisterAsset()
```

and it means the properties-panel **Re-import** button (`AssetPropertiesRenderer.cpp:73`) and the
import dialog are driving one mechanism rather than two. Cancelling the dialog just deletes the
resolved assets.

**Concept naming.** Call the editor-side thing an *import review*, not an "import dialog", and the
engine-side phases *Resolve/Compile/Commit*, not "preview". The second caller is
`EngineCli --batch-import`, which today reimplements Resolve badly (a non-recursive
`directory_iterator("data")` that misses nested engine shaders and therefore remints their UIds —
the gotcha documented in `docs/assets.md`). If Resolve is a real engine function, the CLI's
correctness problem is a deletion rather than a fix.

### Per-directory settings: two levels, only build the first

- **Level 1 (transient, in the dialog):** multi-select rows, edit config for the selection.
  Covers "I dropped a texture pack and 40 of these are normal maps". Cheap, no persistence, no
  format. This is what I'd build.
- **Level 2 (persistent import rules):** a per-project rules file mapping globs → config, applied
  automatically on every import and re-applied on re-import. This is Unity's Presets/AssetPostprocessor
  territory. It is a real format with real versioning cost, and it only earns its keep once you're
  re-importing packs regularly. **Flagging, not building.** The one thing to get right now so Level 2
  stays possible: don't let the review list mutate config in a way that erases *why* a value was
  chosen — see §4.

### Gap that affects both

Textures pulled in as model dependencies get `CopySourceFiles` force-disabled
(`AssetImporter.cpp:242-246`), so unlike top-level drops they have no copy under `<project>/content/`
and their `AssetSource` points at wherever the user dropped the pack from. Change a setting and hit
re-import six months later and it fails. Worth fixing when the phases are split — it's one line in
the wrong place, but it silently halves the value of per-asset settings.

## 3. Progress

### What's ready

`Pine::Threading` is a better fit than it first looks: worker pool + a **main-thread queue** with
`TaskThreadingMode::MainThread`, pools with `JobsRemaining` counters, and `NotifyMainThreadUpdates`
so a pool can be woken when main-thread work lands. `Assets::LoadAssetsFromDirectory` already uses
this pattern, and `Texture2D::LoadAssetData` already knows the GL upload must be a main-thread task
(`Texture2D.cpp:41`). Per-import status (`AssetImportStatus`) already exists, so a progress bar has
its data source.

### Three things in the way

1. **`Run()` is a serial `for` loop** (`AssetImporter.cpp:165`). nvtt BC7 compression is the
   expensive part and is embarrassingly parallel per asset.
2. **`PumpMainThreadTasks` is never called from the frame loop** — only from inside
   `AwaitTaskResult`/`AwaitTaskPool` (`Threading.cpp:240,260`). So today the *only* way main-thread
   tasks run is if someone blocks waiting for them, which is exactly what a progress bar must not
   do. `Texture2D::LoadAssetData` queues its GL upload and then immediately `AwaitTaskResult`s it
   (`Texture2D.cpp:113`) — fine while everything is synchronous, deadlock-shaped the moment it isn't.
   **Calling `PumpMainThreadTasks()` once per frame from the editor loop is worth doing regardless
   of this feature**; it's the missing half of the threading design.
3. **`ImportRelative` mutates the shared `Imports` vector while `Run()` is iterating it**
   (`AssetImporter.cpp:248`) — unsafe under any parallelism, and already a latent crash serially (§5).

### Recommended split

Compile is already almost thread-clean: `Asset::Import()` for textures is pure nvtt/CPU, for models
it's assimp — no GL, no global state. Commit is not: it writes the `.passet`, uploads to GL, and
mutates `m_AssetsMapPath/UId`. **That seam already exists in the code; it just isn't named.** So:

- Resolve on the main thread (synchronous, fast).
- Compile as N pool tasks. Model dependency textures get hoisted into Resolve so the work set is
  fixed before any thread starts and `ImportRelative`'s dynamic insertion disappears.
- Commit drained on the main thread, a bounded number per frame, so the editor keeps painting.
- The modal reads `pool->JobsRemaining` and the per-import statuses. Non-blocking; the frame loop
  keeps running.

If you want something in the first batch without touching threading: make `Execute` resumable
(`Importer::ExecuteStep(context, budget)` called each frame from the modal). Same UI, same phase
split, no data races, and it upgrades to the parallel version later without the dialog changing.
Given 2531 assets in `gm/assets` today, even a serial-but-responsive import is a large improvement
over a frozen window.

## 4. A smarter importer

The interesting part isn't the heuristic, it's that **there are already two sources of evidence and
the better one is being thrown away.**

`ModelImporter::ImportTexture` (`ModelImporter.cpp:113`) *knows* it's fetching
`aiTextureType_NORMALS` — the model file said so, which is authoritative. It then calls
`ImportRelative`, which passes no configuration, so the texture imports with the default
`AlbedoFaster` hint → **BC1 compression and `SetSRGB(true)`** (`Texture2D.cpp:15`) on a normal map.
That's wrong today, independently of any UX work: normal maps should be BC5 and linear. Same for
specular. A filename heuristic would be an improvement over nothing, but here the engine has ground
truth and discards it.

So the mechanism to build is **usage-hint resolution with ranked evidence**, not "normal map
detection":

```
explicit user config  >  source-format evidence (assimp texture slot)
                      >  name/path heuristic  >  type default
```

Name it for that (`ResolveUsageHint` / `UsageHintEvidence`), because the second caller is obvious
and already exists: `Texture3D`/skybox faces want "never sRGB-decode this as albedo", and a future
glTF importer will carry metallic-roughness slot information the same way assimp carries normals.
A function called `GuessNormalMapFromFilename` cannot take either.

For the heuristic tier itself, keep it a small ordered table of substring rules over
`filename + parent directory names`, matched case-insensitively on token boundaries
(`_n`, `_nrm`, `normal`, `_rough`, `_metal`, `_ao`, `_emis`/`emission`, `_mask`). Concretely,
`gm/assets/Textures/` has ~20 files suffixed `_emission` that all imported as sRGB BC1 albedo.
Token boundaries matter: this same tree has `metal_floor_5.png`, which is an albedo texture of
metal, not a metalness map — a naive `contains("metal")` gets it wrong. Whatever the rules, the
review list from §1 is what makes them safe: a wrong guess is a visible dropdown the user corrects
before anything is written, rather than a silently mis-compressed texture.

Worth noting the ranking has to survive into Level-2 rules later: "the user set this" and "the
importer guessed this" must stay distinguishable, or re-import will clobber manual overrides. One
extra enum on the config, decided now, saves a migration later.

## 5. Bugs found while reading

1. **Use-after-free in `ImportRelative`** (`AssetImporter.cpp:248`). `Run()` passes
   `context->Imports[i]` as `AssetImport& import`. During `asset->Import(&import)`, a model import
   reaches `ImportRelative`, which does `Imports.push_back(...)`. If that reallocates, `import` in
   the caller's frame dangles — and `Import()` then writes `import.ImportStatus` and
   `import.AssetPtr` through it (`:105-106`). It survives today only because `push_back` growth
   usually leaves spare capacity. Fix: stable storage (`std::vector<std::unique_ptr<AssetImport>>`
   or `std::deque`). Required anyway before any of §3.
2. **Re-import destroys UIds** (§1). Re-dropping an already-imported file mints a new UId over the
   same `.passet`, breaking every reference to it. This is the editor equivalent of the
   `EngineCli --import` footgun already documented in `docs/assets.md`.
3. **Model-sourced normal/specular maps import as sRGB BC1 albedo** (§4). Independent of the UX
   work and cheap to fix once `Configuration` actually reaches the asset.

## Suggested sequencing

**Status: batch 1 landed** (importer phase split, stable entry storage, update-in-place, the model
usage-hint fix). The rest below is still open.

**Batch 1 — the seam and the two live bugs.** Stable `AssetImport` storage; split
`Resolve`/`Compile`/`Commit`; make config reach the asset; resolve already-imported → update in
place. No new UI. This is the part that's hard to retrofit and the part that stops the importer
from corrupting references.

**Batch 2 — the review dialog.** List from `Resolve`, per-row and multi-select config editing,
Import/Cancel. Reuses the existing property widgets.

**Batch 3 — progress.** Resumable `Execute` + a modal, or straight to the pool version.
Add the per-frame `PumpMainThreadTasks()` here (or earlier — it's independently correct).

**Batch 4 — evidence-ranked usage hints.** Assimp slot evidence first (it fixes a real quality
bug), filename heuristics second, now safe because the user sees them in the review list.

Persistent per-directory import rules stay out of all four until you're actually re-importing packs
often enough to feel the cost.

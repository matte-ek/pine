# Plan: import review, per-asset settings, progress, smarter hints

Follow-up to [asset-import-ux.md](asset-import-ux.md), whose batch 1 (the Resolve/Execute phase
split, stable entry storage, update-in-place, the model usage-hint fix) has landed.

**Scope decision: imports stay single-threaded.** Progress is a plain "3/58 assets imported"
counter driven from the main thread. That removes the only stage that had real prerequisites — see
"What parallel would have cost" at the end, so the reasoning is on record if it ever comes back.

## Blockers

None. With parallelism out, everything left sits on the seam batch 1 already built. Two small
things are prerequisites only in the sense that a feature is a lie without them.

- **`ImportCompressionQuality` is never persisted.** It's declared in `TextureSerializer`
  (`Texture2D.hpp:86`) but neither `LoadAssetData` nor `SaveAssetData` touches it, so changing it
  in the properties panel survives exactly until reload. "Per-asset import settings" is not a
  feature until this round-trips. Two lines.
- **The settings widgets live inside the properties panel.**
  `AssetPropertiesRenderer::RenderTexture2D` (`AssetPropertiesRenderer.cpp:69-77`) draws them
  inline. The dialog must draw the *same* widgets or the two drift.

And one fact that shapes the UI rather than blocking it: **only `Texture2D` has an
`AssetImportConfiguration` at all** — `Model` and `Shader` have none. The dialog's settings column
is texture-only. That's fine (textures are where the settings matter), but don't build a generic
property grid for one type.

## Stage 0 — prerequisites

1. Read/write `ImportCompressionQuality` in `Texture2D::LoadAssetData`/`SaveAssetData`.
2. Extract the import-settings widgets so the properties panel and the dialog share them. Name it
   for what it is — the settings an asset was imported with — not for the dialog that happens to be
   the second caller.

## Stage 1 — the import review dialog

**Flow.** `Gui.cpp`'s `OnWindowDrop` runs inside `glfwPollEvents`, so it must not import there. It
stashes the dropped paths; the next frame builds the context, calls `Importer::Resolve`, and opens
a modal. Lives in `Editor/src/Gui/Dialogs/AssetImport/`.

**Content.** One row per `AssetImport`: source file, `Type`, `ResolvedEnginePath`, and `Action` in
plain language — *New*, *Replaces existing (keeps UId)*, *Unsupported type*, *Blocked: something
else is already there*. The last two are most of the payoff; they are currently `PError`s
scrolling past in the console.

**Editing.** Row selection (including multi-select) drives the extracted widgets, writing straight
into `AssetPtr`'s configuration. No new plumbing — that is what batch 1's early asset construction
bought.

**The wrinkle to get right.** For `Action::Update`, `AssetPtr` is a *live, loaded asset*. Editing
its configuration mutates the real thing before the user has agreed to anything, and Cancel must
not leave that behind. Snapshot the configuration of every `Update` row at resolve time and restore
it on cancel. (`Create` rows need nothing — `DeleteContext` already deletes assets the context
still owns.)

**Not in scope.** Thumbnails: `IconStorage` renders icons for *loaded* assets, and resolved rows
aren't loaded yet. Type icons only.

## Stage 2 — progress

Add a cursor to `ImportContext` and one function:

```
bool Importer::ExecuteNext(ImportContext* context);   // false once the cursor reaches the end
```

It advances past entries `Resolve` marked `Unsupported`/`Conflict` as well as ones it imports, so
the cursor is the progress. The editor owns the pacing — the engine importer has no business
knowing about frame budgets. The dialog calls `ExecuteNext` in a loop each frame until ~8ms is
spent (always at least one, or a big BC7 texture would stall forever) and draws the counter.

**The denominator is the row count the dialog already showed**, not `Imports.size()`. Those differ:
a model discovers its textures during compile and appends them to the queue, so `Imports.size()`
grows as you go — a counter reading "3/58" would climb to "3/2400" on a pack of `.glb` files.
Counting the files the user actually dropped keeps the number honest; the dependencies just make
individual steps take longer.

That's the whole feature. No threading, no locking, no data races, and the existing
`AwaitTaskResult` inside `Texture2D::LoadAssetData` already pumps main-thread GL tasks correctly
when called from the main thread — so the per-frame `PumpMainThreadTasks()` the earlier draft
wanted isn't needed either.

## Stage 3 — evidence-ranked usage hints

Independent of stages 1–2; can go first if the smarter importer is what you want soonest.

The mechanism is **hint resolution over ranked evidence**, not normal-map detection:

```
User (set by hand)  >  SourceFormat (assimp texture slot)  >  Heuristic (name/path)  >  Default
```

Store the winning source alongside the hint on the configuration and serialize it. That's
backwards compatible: the serializer is name-based and `DataPrimitive` tracks `m_Populated`, so
`Read()` leaves the caller's default alone when an older `.passet` lacks the field. No migration.

The rank is what makes re-import safe. Batch 1 currently avoids clobbering a hand-set hint by
refusing to configure `Update` rows at all — a blunt version of the same rule, to be replaced by
the real one here.

**Heuristic tier.** A small ordered table of token-boundary matches over the filename plus parent
directory names: `_n`, `_nrm`, `normal`, `_rough`, `_metal`, `_ao`, `_emis`/`emission`, `_mask`.
Token boundaries are not optional: `gm/assets/Textures/` contains ~20 `*_emission` files that all
imported as sRGB BC1 (the case this catches) *and* `metal_floor_5.png`, an albedo texture of metal
that a naive `contains("metal")` would wreck. Stage 1 is what makes a wrong guess cheap — a
dropdown to correct, not a silently mis-compressed texture.

**Two gaps this stage has to decide about:**

- **Assimp maps OBJ `map_Bump` to `aiTextureType_HEIGHT`, not `NORMALS`** (verified). Batch 1's fix
  therefore does nothing for OBJ models — irrelevant for `gm` (all `.glb`) but a trap later.
  Whether `HEIGHT` counts as normal-map evidence is a per-format judgement, which is exactly what
  an evidence table should encode instead of a hardcoded switch.
- **There is no usage hint for a compressed *linear colour* texture.** The options are BC7/BC1
  (both sRGB), BC5 (2-channel), BC4 (1-channel), or uncompressed. Specular/roughness/AO maps have
  nothing correct to map to, which is why batch 1 mapped only normals. Adding a linear BC7 hint is
  the enabling change and should land with this stage, not after it.

## Not planned

**Persistent import rules** (per-project glob → settings, re-applied on re-import). Unity's Presets.
A real file format with real versioning cost, and stage 3's evidence rank is most of its value at
none of it. Revisit when you're re-importing packs often enough to feel it.

**Parallel import.** For the record, what it would have cost — all of it engine-wide rather than
importer-local:

- `Assets::GetAssetByUId`/`GetAssetByPath` (`Assets.cpp:231-249`) read the asset maps with **no
  lock** while `RegisterAsset` writes them under `m_AssetsMutex`. Safe today purely by accident:
  the only threaded asset work is `LoadAssetsFromDirectory`, which blocks the main thread for its
  whole duration. A background import breaks that, because `AssetHandle::Get()`'s cache-miss path
  reads those maps during `RenderManager::Run()`.
- `UId::New()` (`UId.cpp:61`) mutates a function-local `static std::mt19937_64` unsynchronised.
  Every `Create` resolve mints a UId, and a corrupted RNG means duplicate UIds — which in this
  engine means assets silently aliasing each other.
- `ImportRelative` would have to become resolve-only during a model's compile, so GL work stays off
  workers. (This one is *free*: `AssetHandle::operator=(Asset*)` only stores the UId and caches the
  pointer, no manager lookup, so a resolved-but-uncompiled texture is already enough for
  `Material::SetDiffuse`.)

None of that is hard, and the locking is wanted anyway the day assets stream in the background.
It just isn't worth paying for to speed up an operation performed rarely.

---

## Sequencing

**0 → 1 → 2 → 3.** Stage 3 can jump the queue if the guessing matters more than the dialog.

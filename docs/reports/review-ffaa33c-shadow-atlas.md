# Review: `ffaa33c` — "Add spot and point light shadows, and fold the cascades in with them"

Read-only review of one commit, at the state it landed in, checked against the tree at `fa0260e`.
Where a later commit already moved something on, that's noted. The point is what the commit did to
the renderer, which of its assumptions are still load-bearing, and what is safe to leave alone.

Nothing here has been changed. Findings are ordered by confidence, and a suggested order of work is
at the end.

## What actually landed

The shape of the change is one idea applied everywhere: **a shadow is no longer something a light
type owns, it is a `ShadowView`**, and by the time anything draws, there are no light types left.

**1. `Rendering/ShadowView/`** — a projection, its frustum, a slice of a render target, a visibility
set, and a bias/face-culling policy. A directional light decomposes into `CASCADE_COUNT` of them, a
spot into one, a point light into six. `RenderViews()` (`Shadows.cpp:785`) is now the only place
shadow depth is drawn anywhere in the engine, and it has no per-light-type branch. The type lives
outside `Features/Shadows/` deliberately, so a reflection probe or an offline bake could reuse it.

**2. `Features/Shadows/ShadowAtlas/`** — one D16 texture, split into quadrants of uniform tile size
(1 Half + 1 Half + 4 Quarter + 16 Eighth = 22 tiles). Mark-and-sweep per frame, so a tile is stable
for as long as its owner keeps asking and free the moment it stops. Group acquires are all-or-nothing.
Pinned reservations are the exception, and exist for the cascades. This replaces a separate
4096×4096×2 cascade array plus a 2048 local atlas with a single 4096 D16 surface.

**3. Caching.** A `TileState` per slot, invalidated by a changed view-projection, by
`SceneProcessor::CasterSetChanged`, or by a mover whose old *or* new bounds intersect the view
frustum. This is why `SceneProcessor` gained `MovedCasters` and `ModelRendererHintData::PreviousBounds*`,
and why `EndFrame()` was split out of `Prepare()` — the dirty-flag clear had to survive past the
shadow pass. The decision not to drive this from `Entity`/`Transform` dirty flags is correct and is
documented at `SceneProcessor.hpp`; the flags mean different things in editor and production mode,
and nothing clears a `Light`'s at all.

**4. Scheduling split.** Local views are built and rendered **once per frame** in `Pipeline3D::Prepare`,
because a spot or point light's shadow map does not depend on the viewer. Cascades stay **per
rendering context** in the prepass, because they are derived from the camera frustum. With an editor
viewport and a game camera both live, this is the main wall-clock win in the commit.

**5. Renderer3D surface changes.** `AddDirectionalShadowMap` and the `hasDirectionalShadowMap`
uniform are gone; the atlas is bound unconditionally and a non-casting light carries
`shadowViewIndex = -1`. The `Shadows` UBO (binding 4) became `ShadowViews` (binding 7). `AddLight`'s
spot branch now scans for a free slot instead of writing `SPOT_LIGHT_OFFSET` directly — which was
correct only while there was exactly one spot slot, and this commit raises it to two.

**6. `Light::Attenuation` → `Light::Range`.** The constant/linear/quadratic curve had no zero, so
lights had to be treated as infinite. The replacement is a windowed inverse-square falloff that
reaches exactly zero at `range`. This is not cosmetic: it is what makes a shadow far plane at that
distance legitimate, since any illumination surviving past the range is illumination the shadow pass
never rendered casters for.

**7. `Pipeline3D::RenderBatch` went public** and terrain was lifted out of it into `RenderTerrain()`,
so the shadow pass reuses the real batch renderer instead of the near-copy it used to keep. Terrain
had to come out because it renders through its own path and ignores `Renderer3D`'s shader override —
inside a shadow view it would have drawn itself lit into the depth target.

**8. Cascade projection fixes, unannounced in the summary but significant.** `glm::ortho` takes
zNear/zFar as positive distances along the view direction; light space puts everything at negative z.
The two were passed straight through and only produced a usable box because the light eye sits one
unit from the box centre. Both ends are explicit now, and the near plane is fitted to the casters
that can actually reach the box (`FitCasterNearZ`) rather than clipped at the box's own front face —
which is what used to make shadows wink out when the camera turned.

Architecturally this is a good change. The abstraction is the right one, the scheduling split is
right, and the deletion it bought is real.

## Findings

### HIGH CONFIDENCE — a high-importance spot light can silently get no shadow while 16 tiles sit free

`SelectTileSize` (`Shadows.cpp:422`) clamps the requested class against `GetTileCapacity`, which its
own comment states is *capacity, deliberately, and not availability*. At importance ≥ 1.0 a spot
light asks for `Quarter`, and there are only four `Quarter` tiles in the layout.

The fifth important spot light hits `ShadowAtlas::Acquire` returning false at `Shadows.cpp:1011` and
does `continue`. There is no retry at a smaller class. It then holds nothing, so next frame
`FindLightState` returns `nullptr`, so the hysteresis threshold resets to `HIGH_RES_TILE_IMPORTANCE`,
so it asks for `Quarter` again and fails again. The light is permanently unshadowed while sixteen
`Eighth` tiles sit empty.

This is exactly the failure mode described in the comment above `SelectTileSize` — *"a request that
can never be granted… it holds nothing, so it has no state to be demoted from, so it asks for the
same impossible thing again next frame"*. The point-light half of that problem was fixed by the
capacity clamp; the contention half was not.

The debug slot limit would not have surfaced this: it caps claims globally and denies every light
equally, so the asymmetry between "this class is full" and "the atlas is full" never shows up.

Fix is small: on a failed `Acquire`, retry once at `TileSize::Eighth` before giving up.

### HIGH CONFIDENCE — `Light::CastShadows` does nothing for directional lights

`SelectShadowCandidates` checks it (`Shadows.cpp:460`), but that branch handles only spot and point
lights. `RenderPassLight` (`Shadows.cpp:1199`) tests `GetLightType() == Directional &&
m_CascadeSlotsReserved` and never consults `GetCastShadows()`.

The editor shows the checkbox for every light type — `ComponentPropertiesRenderer.cpp` renders it
unconditionally, unlike the `Range` slider directly above it, which is correctly gated on
`!= Directional`. So unticking "Cast Shadows" on the sun silently does nothing.

Either gate the checkbox out for directional lights, or honour it in `RenderPassLight`. The second is
the more useful behaviour and is a one-line condition.

### HIGH CONFIDENCE — `Light::shadowFade` is dead, and its documentation is wrong three times

The field is declared in `common.glsl:39`, in `ShaderStorages.hpp:83`, and written at
`Renderer3D.cpp:441`. No shader reads it. The fade that actually reaches the image rides in
`shadowViews[].params.z` and is applied by `SampleShadowView`.

Worse, it is written as `ShadowViewIndex >= 0 ? 1.f : 0.f`, so it never holds the 0..1 value that all
three of its comments promise.

Its comment — *"Lives here from the start on purpose: retrofitting a factor the shader must multiply
by, after the lookup already works, means finding every place that forgot to"* — is the speculative
generality argument, and in this case it lost: the fade ended up belonging to the view, not to the
light, because a point light's six faces have to fade together. Delete the field from all three
places.

### MEDIUM CONFIDENCE — dead abstraction in `ShadowView` and a new pure virtual with no callers

Three inert things landed together, all from the array-texture cascade path that the atlas obsoleted
inside this same commit:

- `ShadowView::Target` is assigned in three places (`Shadows.cpp:588`, `:650`, `:738`) and read
  nowhere. `RenderViews` calls `ShadowAtlas::GetFrameBuffer()` directly.
- `ShadowView::TargetLayer` is only ever assigned `-1`.
- `IFrameBuffer::AttachTextureLayer` was **added as a pure virtual in this commit** and has zero
  callers. Every future backend now has to implement a method nothing asks for.

The "a reflection probe would want this" justification in `ShadowView.hpp` is reasonable, but the
reflection probe does not exist and the renderer does not honour the indirection today — `RenderViews`
hardcodes the atlas. Either make `RenderViews` actually bind `view.Target` (cheap, and makes the
generality real), or drop the two fields and the interface method until something needs them.

`TextureFormat::Depth32F` is also unused, but that one is fine — it is the paired half of `Depth16`
and costs one switch case.

### MEDIUM CONFIDENCE — the `Low` preset contradicts its own documentation

`GraphicsSettings.hpp` says of `LocalShadowTileBudget`:

> A point light costs six of these, so anything under 6 silently means "point lights never cast" —
> which is why the presets step 6 / 12 / 16 rather than 2 / 4 / 8.

`ApplyPreset` then sets `Low` to **2**. Four presets, three numbers in the comment.

If "no point-light shadows on Low" is the intended trade, that is defensible — but the comment
currently reads as though no preset does that, which makes the Low setting look like an oversight
rather than a decision. Either raise it to 6 or say explicitly that Low opts out.

### MEDIUM CONFIDENCE — `Statistics::TilesRendered` mixes cascades with local tiles

`TilesRendered` is incremented inside the shared `RenderViews` (`Shadows.cpp:847`), so it counts
cascade tiles too — and cascades render per rendering context, so with an editor viewport and a game
camera both live it picks up `2 × CASCADE_COUNT` on top of the local tiles.

`TilesCached`, by contrast, is `m_LocalViewCount - staleViews` and is local-only. The debug panel
prints the two adjacent to each other, so `rendered + cached` does not reconcile against
`LocalViewCount`. The panel comment claims *"a single number is the honest one"*, which would be true
if the cached figure were also whole-frame.

`CascadeViewCount` is already tracked separately, so the data to separate them is there.

### MEDIUM CONFIDENCE — `BuildSpotView`/`BuildPointView` do not set every field they depend on

Both fill an entry of `m_LocalViews` in place — the vector grows but never shrinks and its indices
are reused across frames by different lights, which is deliberate and documented (the `VisibilitySet`
allocation is what is being preserved).

But neither sets `FaceCulling`. It works because local views leave it at the struct default (`Back`)
and cascades live in a separate vector (`m_CascadeViews`) where `Front` is set explicitly. That is an
invariant nothing states and nothing enforces, and it breaks the first time the two vectors merge —
which is precisely the direction the `ShadowView` design is aiming at. Set it explicitly in both
builders.

### LOW CONFIDENCE / QUESTION — lifetime and bounds

- **`ShadowAtlas::Shutdown` leaks the texture.** It destroys the framebuffer but never calls
  `DestroyTexture` on `m_Texture`, and does not null it. `Shadows::Shutdown` likewise leaves
  `m_CascadeSlotsReserved`, `m_TileStates` and `m_LightStates` untouched. Harmless at process exit;
  broken if Setup/Shutdown ever cycle — which is exactly what applying `ShadowAtlasResolution`
  without a restart would need. Is one-shot lifetime the intended assumption?
- **`Acquire`, `GetViewport`, `GetUvRect` and `MarkRendered` do not bounds-check the slot index**,
  while `Shadows::GetTileDebugInfo` does. Every caller is disciplined today; the inconsistency is the
  observation.
- **The debug panel casts `slot.Owner` to `const Pine::Light*`** (`DebugPanel.cpp`). This is safe as
  written — `ShadowAtlas::EndFrame` sweeps in `Pipeline3D::Prepare`, before the UI draws, so a
  surviving owner is a light that claimed this frame — but it is the one place that reconstructs a
  typed pointer from the deliberately opaque `const void*`, and the safety argument rests on frame
  ordering that nothing near the cast mentions. `TileState::Owner` carries a comment warning that the
  component pool reuses slots and a new `Light` can be constructed at a dead one's address; the panel
  has the same exposure and no such note.

### LOW CONFIDENCE / QUESTION — smaller asymmetries

- **`SetCastShadows` does not raise the entity dirty flag** while `SetRange` and `SetLightType` do.
  I believe it is fine — candidate selection is rebuilt from scratch every frame and a newly-claimed
  tile starts with `ContentValid == false` — but it is an unexplained difference between three
  adjacent setters in the same file.
- **A fading-out loser still consumes atlas space.** In `PrepareLocalViews`, a light that lost but
  still has `Fade > 0` re-acquires its tiles and is deliberately not charged to `tilesSpent`. True of
  the budget, but it still occupies real tiles, so it can starve a genuinely visible light via a
  failed `Acquire`. Bounded by `FADE_SECONDS`, so this is a note rather than a defect.
- **`struct ShadowConfiguration {}` in `Shadows.hpp:24` is empty and unreferenced.**
- **`Samplers::SHADOW_ATLAS` stayed at 17** with a comment noting that slot 16 was freed when the
  cascades' own array texture went away. Moving it to 16 would need a `.passet` re-import, so leaving
  it is reasonable — just noting the gap is now intentional and undocumented at the constant itself.

## The shared C++/GLSL constants

This is the one the commit half-solved, and it is worth treating as its own piece of work.

`Shader::CompileShader` now injects three array sizes as `#define`s after the `#version` line. The
motivation given is sound and expensive — `MAX_INSTANCE_COUNT` was 512 in C++ while the shaders
declared `instances[128]`, and instances 128–511 read out of bounds with nothing reporting it.

Two problems with where it landed.

**It inverts the layering.** `Engine/src/Pine/Assets/Shader/Shader.cpp` now includes
`Pine/Rendering/Renderer3D/Specifications.hpp`. The asset layer, which loads and compiles *every*
shader in the engine including 2D and compute ones, reaches up into the 3D renderer and injects its
vocabulary into all of them. Adding a fourth constant means editing `Shader.cpp` again.

**It covers three of the constants that are actually duplicated.** The full picture:

| Constant | C++ | GLSL | Synced? |
| --- | --- | --- | --- |
| `MAX_INSTANCE_COUNT`, `DYNAMIC_LIGHT_COUNT`, `SHADOW_VIEW_COUNT` | `Specifications.hpp` | injected `#define` | yes |
| UBO block bindings | `Specifications::ShaderStorages` | matched by block *name* | yes, by name |
| Sampler bindings | `Samplers::SHADOW_ATLAS = 17` | `#shader bind ShadowAtlas 17` | **no — literal** |
| Cascade split distance | `farPlane[0] = 10.f` (`Shadows.cpp:706`) | `fragCameraDistance > 10.0` (`shadows.glsl:176`) | **no — literal, two files** |
| `CASCADE_COUNT` | `Specifications.hpp` | implicit in the `> 10.0` test | **no** |
| `ObjectLightSlots::POINT_LIGHT_COUNT` / `SPOT_LIGHT_COUNT` | `Specifications.hpp` | hand-unrolled `lightIndices[0..6]` | **no** |

The cascade split is the sharpest case: `Specifications.hpp` carries the comment *"Changing this will
require manual configuration, configure ranges Shadows.cpp and the rendering shader"* — a documented
manual sync, in the same commit that introduced a mechanism for exactly this and did not use it.

### A shader-define registry

The shape that fits Pine's conventions — a namespace subsystem with its state in an anonymous
namespace:

```
Engine/src/Pine/Graphics/ShaderDefines/ShaderDefines.{hpp,cpp}
namespace Pine::Graphics::ShaderDefines

void Set(const std::string& name, int value);
void Set(const std::string& name, float value);
const std::string& GetDefineBlock();   // cached, rebuilt on Set
```

`Shader::CompileShader` inserts `GetDefineBlock()` after `#version` and drops the `Specifications.hpp`
include entirely, so `Assets` stops depending on `Rendering`. Each subsystem then registers what it
owns at its own setup: `Renderer3D` the instance and light counts and the object light slots,
`Shadows` the view count, cascade count and split distances. The dependency is inverted and adding a
constant touches one file.

**The ordering trap, which is the real design constraint.** From `Engine.cpp`:

```
104:  Assets::LoadAssetsFromDirectory("engine")   // compiles every engine shader here
122:  Rendering::GraphicsSettings::Setup();
124:  RenderManager::Setup();                     // -> Pipeline3D::Setup -> Shadows::Setup
125:  Renderer3D::Setup();
```

`Shader::LoadAssetData` compiles version 0 synchronously on load. If `Renderer3D` and `Shadows`
register their defines in their `Setup()`, **every engine shader has already been compiled against an
empty registry.** The current hardcoded version dodges this precisely because `constexpr` values need
no registration step. So this is not a drop-in refactor. Two ways out:

- **(a) Register before `Assets::Setup()`** — a small `Rendering::RegisterShaderDefines()` called
  early in `Engine::Setup`. Simplest, keeps everything effectively compile-time, but splits
  "Rendering's setup" across two call sites for a reason that needs a comment to be obvious. The
  awkwardness is already latent: `Renderer3D::Setup()` runs *after* `RenderManager::Setup()` today,
  which is its own ordering oddity.
- **(b) Revision counter + lazy recompile** — the registry bumps a revision on every `Set`, `Shader`
  records the revision it compiled at, and anything stale recompiles on first `GetProgram`. More
  moving parts, but the machinery already exists (`CompileShaderVersion`, plus `Utilities::HotReload`),
  and it makes registration order genuinely not matter. That last property is what would make a
  *dynamic* registry worth more than a static header — pick this one if subsystems will ever register
  conditionally, e.g. a feature that is switched off registering nothing.

Start with (a); reach for (b) when a define needs to change at runtime.

**Two scope boundaries worth drawing up front:**

1. **Leave `#shader bind` out of v1.** Those are resolved at *import* time in
   `ShaderImporter::ProcessShaderLine` and baked into the `.passet`, not at compile time. Making
   `#shader bind ShadowAtlas SHADOW_ATLAS` work means populating the registry inside `EngineCli`
   during import as well, and re-baking every asset when a constant changes. Real, but a second step.
2. **Do not try to generate the hand-unrolled light slots.** The unrolling exists because dynamically
   indexing the `vIn.lightDir[]` *varying* array misbehaves on NVIDIA (documented in
   `generic.vertex.glsl` and `lightning.glsl`). A `#define` can give the shader the count, but the
   unrolled writes still have to be hand-written or genuinely code-generated, and codegen is a much
   larger commitment than a define registry.

One decision to make deliberately rather than inherit: every shader in the engine, `Pipeline2D`'s
included, currently receives the 3D renderer's defines. Unused `#define`s are harmless, so scoping is
probably over-engineering today — but it should be a choice.

## On code quality, against the guidelines that now exist

The engine-facing work is strong and would pass the current guidelines. `ShadowView` is the right
seam, named for what it is; the allocator is deliberately dumber than a packer and says so; the
scheduling split between per-frame and per-context is the kind of decision that is invisible when
right and expensive when wrong. The `RenderBatch` extraction deleted a near-copy rather than growing
a second one, which is what `design-for-change.md` asks for.

Two habits are worth naming.

**Comment volume.** `ShadowView.hpp` is 81 lines, of which roughly 55 are prose. `ShadowAtlas.hpp`
explains the all-or-nothing acquire three times — in the header, again at `Shadows.cpp:1009`, and
again in the commit message. A lot of this is genuinely valuable: the `glm::ortho` convention note,
the NVIDIA varying-array warning, the reasoning for bounds-over-dirty-flags are all things that would
otherwise be rediscovered painfully. But much of the rest argues with alternatives that were never
implemented, or justifies one decision at the declaration, the definition and the call site.

A workable rule: keep the comment if it records a constraint, a measurement, or a bug that was
actually paid for. Cut it if it is arguing against a design nobody wrote.

**Comments that outlive their truth.** This is the same habit flagged in the `357fa72` review, and it
recurred. `shadowFade` is described correctly in three places and implemented in none of them. The
`LocalShadowTileBudget` comment contradicts the `Low` preset twelve lines below it. `Specifications.hpp`
still tells you to hand-sync `CASCADE_COUNT` into the shader. Because these read as authoritative,
they make the defects *harder* to spot than no comment would have. A comment asserting a guarantee is
worth a moment spent checking the guarantee holds.

Neither habit is a reason to write fewer comments here — the density is mostly earned. It is a reason
to treat a comment as code that can rot.

## Suggested order

If this gets picked up in pieces:

1. **`SelectTileSize` fallback** and **directional `CastShadows`** — small, self-contained, both are
   visible wrong behaviour.
2. **Delete `shadowFade`**, the empty `ShadowConfiguration`, and either wire up or remove
   `ShadowView::Target` / `TargetLayer` / `AttachTextureLayer`. Pure deletion, no behaviour change.
3. **Reconcile the `Low` preset with its comment**, fix `TilesRendered`, set `FaceCulling` explicitly
   in both local view builders.
4. **The shader-define registry.** Its own change, with the boot-order question settled first. The
   cascade split distance is the constant that most wants it.
5. **Lifetime cleanup in `ShadowAtlas::Shutdown` / `Shadows::Shutdown`** — only worth doing if
   runtime re-initialisation of the atlas becomes a goal.

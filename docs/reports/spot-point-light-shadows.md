# Spot & point light shadows

Status: analysis only, nothing changed. Verified against `ai` @ 6994881 plus the uncommitted
lighting/culling work in the tree (2026-09-04).

**Short version.** The expensive part of point light shadows is not rendering six depth faces —
it is (a) that a forward renderer pays the shadow lookup *per fragment per light slot*, and (b)
that six faces per light re-walk the whole draw batch every frame. Neither is fixed by a better
depth pass. The two things that actually make this affordable are **caching** (a light's depth
faces are only re-rendered when something in that face moved) and a **budget** (a bounded number
of shadow tiles, handed out by importance). Both of those want the same underlying object, which
the engine does not have yet: a *shadow view* — one matrix, one frustum, one region of one texture,
one visibility set. Cascades, spot cones and cube faces are all that same thing, and the new
`Frustum` / `VisibilitySet` pair is already shaped to take it.

So the recommendation is not "add a cube map path". It is: introduce `ShadowView` + a tiled
`ShadowAtlas`, express the *existing* cascades in those terms, and then spot (1 view) and point
(6 views) cost almost no new machinery.

---

## 1. What the current path actually is

`Rendering/Features/Shadows/` presents a per-light API — `RenderPassLight(light, batch)`,
`UploadShadowData(light)`, `GetShadowMap(light)` — and then all three functions branch on
`LightType::Directional` and do nothing otherwise. The API shape is right; the *return type* of
`GetShadowMap` is the thing that will not survive: it hands back one whole `ITexture*` per light.
An atlas has one texture and N regions, so the honest signature is a texture plus a view index,
which is a different call. Worth changing before there is a second caller, not after.

Below the API, three assumptions are load-bearing and each of them is single-light-shaped:

**One depth target, one sampler, one uniform.** `Renderer3D` holds `m_DirectionalShadowMap` and
`m_HasDirectionalShadowMap` as file-scope singletons, binds the map at sampler slot
`Samplers::DIRECTIONAL_SHADOW_MAP = 16`, and toggles one `hasDirectionalShadowMap` uniform. There
is no room in this for "this object's five point lights have shadows and its spot does not". The
per-light answer has to come from the light buffer, not from renderer state.

**The `Shadows` UBO is `mat4 lightSpaceMatrix[8]`, indexed by cascade.** The fragment shader picks
`cascadeIndex` from camera distance and multiplies. That array is the right *size* to be repurposed
but the wrong *meaning*: it is indexed by cascade, and what is needed is an index that a light can
name.

**Cascades are rendered with a layered geometry shader** (`shadow.geometry.glsl`,
`invocations = 2`, `gl_Layer = gl_InvocationID`) into a `Texture2DArray`. This is a genuinely good
fit for cascades that share a caster set — and a genuinely bad fit for anything wanting per-view
culling, because the geometry shader duplicates *every* triangle into *every* layer. Cascade 0
covers 10 units and currently redraws everything within ~38.7 units. That is the existing cost this
report's proposal happens to remove as a side effect.

Two smaller things worth knowing before touching this file:

- `Specifications::Shadows::MAX_SHADOW_DISTANCE = 1500.f` is used **squared** in `RenderScene`
  (`glm::distance2(...) > MAX_SHADOW_DISTANCE`) and **un-squared** under a `sqrtf` for the cascade
  far plane. Both agree on ~38.7 world units, so the behaviour is consistent — the name is what
  lies. A local-light shadow distance fade will read this constant and get it wrong.
- Shadow rendering happens inside `Pipeline3D::Run(..., PipelineStage::Prepass)`, i.e. **once per
  rendering context**. With the editor viewport plus a game camera that is two full cascade builds
  per frame into *the same* framebuffer. It is correct today only because `RenderManager::Run`
  interleaves prepass and default per context, so each context consumes its own maps before the
  next overwrites them. Anything that later batches all prepasses together breaks this silently.
  For local lights this matters more than for cascades: **a spot or point shadow map does not
  depend on the viewer at all**, so rendering it per context is pure duplicated work. Local shadow
  views belong in `Pipeline3D::Prepare()` (scene-level, once), cascades stay per context.

## 2. Where the performance actually goes

Three separate cost centres, in the order they will hurt:

**(a) Per-fragment lookup cost — the wall.** Pine is a forward renderer with 5 point + 1 spot slots
per object. If every slot can cast, a lit fragment does up to 7 shadow lookups. At the current
directional 3×3 PCF kernel that is 63 texture fetches per fragment. This is the reason engines with
many shadowed local lights are deferred or clustered. Concretely, for Pine:

- Local lights should use a *much* cheaper filter than the directional one. A single
  `sampler2DShadow` fetch with `GL_LINEAR` + `GL_COMPARE_REF_TO_TEXTURE` is already 2×2 hardware
  PCF for one fetch. That is the right default for a point light 8 metres away in a corridor.
- Shadow casting must be **opt-in per light** with a budget, not a property every light has. Unity
  and Unreal both do exactly this, and for the PSX-styled horror slice it is also the correct
  authoring model: two or three lights in a room carry the shadows, the rest are fill.
- The soft-shadow quality knob for local lights should be a shader version or a graphics setting,
  not a per-light field, so the cost is bounded by the preset rather than by the level author.

**(b) View count × batch walk.** `Shadows::RenderScene` walks the entire `ObjectBatchMap`, calls
`Transform::OnRender` per object, and distance-tests each one. Six faces per point light means six
of those walks, per light, per frame. This is what makes "just render a cube map per light" fall
over at four lights, and it is why caching is the headline and per-face culling is only second.

**(c) Depth rasterisation.** Genuinely the cheapest of the three at these resolutions, and the one
per-view frustum culling attacks. Worth doing because it is nearly free given `Cull()` already
exists — but do not expect it to be the win on its own.

## 3. Proposed shape

### `ShadowView` — the unit everything else is built from

```
struct ShadowView
{
    Matrix4f ViewProjection;
    Frustum  Frustum;              // FromViewProjection(ViewProjection)

    // Where it renders. Not just an atlas tile: cascades render into a layer of their own array
    // (see 8.3), and this is what a second atlas or an offline bake target would need too.
    Graphics::IFrameBuffer* Target;
    int      TargetLayer;
    Vector4i Viewport;             // x, y, w, h in target texels

    float    DepthBias, NormalBias;
    RenderCulling::VisibilitySet Visibility;
};
```

One matrix, one frustum, one region of one target, one visibility set. Every shadow source produces
1..N of them:

| source | views |
|---|---|
| directional | `CASCADE_COUNT` (2 today), one per array layer |
| spot | 1, FOV = 2 × outer cone angle, far = range |
| point | 6, FOV = 90°, far = range |

The render loop then has no per-light-type code at all:

```
for (view : activeViews)
    Cull(view.Frustum, view.Visibility)
    SetViewport(view.AtlasTile)
    RenderBatch(batch, view.Visibility)     // the Pipeline3D one, not a shadow-private copy
```

That is the test the naming has to pass: a second, different caller uses this unchanged. A
reflection probe's six faces, a light-probe bake, an offline lightmap rasteriser all want
"a projection, its frustum, a slice of a render target, and who is visible in it" — none of them
would want to rename `ShadowView` to use it, and the only thing that reads as shadow-specific is
the bias pair. Naming it `ShadowFace` or `CubeFace` would fail that test immediately.

Note also that `RenderBatch` in `Pipeline3D.cpp` already takes `(batch, mode, visibility)`. Shadows
having its own private `RenderScene` copy is the duplication to remove here, not to extend.

### `ShadowAtlas` — one texture, tiles handed out by importance

A single depth texture (2048² or 4096²) partitioned into power-of-two tiles. Recommended over the
alternatives:

- **vs. a cube map array** (`samplerCubeArrayShadow`, core in GL 4.0, so the `#version 420 core`
  shaders can use it): hardware face selection and seamless filtering are real advantages, and it
  is the *simpler shader*. But every slice is the same resolution, so a lamp filling the screen and
  one at the end of a corridor cost the same; memory is brutal (32 lights × 6 × 512² × 4B ≈ 200 MB,
  so slices have to be budgeted anyway — at which point you have an allocator, just a worse one);
  and it needs a `TextureType::CubeMapArray` added to the graphics abstraction plus a layered
  attach path. It also does nothing for spot lights or cascades, so it is a third shadow mechanism
  rather than one.
- **vs. dual paraboloid**: half the passes, but the distortion needs tessellated geometry to look
  right and it interacts badly with the low-poly aesthetic. Not worth it.
- **The atlas** costs one thing the cube array does not: manual cube-face selection and edge
  handling (see §4). In exchange it gives per-light resolution, one sampler, one allocator shared
  by cascades/spot/point, and — the part that matters most — **stable tiles across frames**, which
  is the precondition for caching.

Allocation must be *stable*, not repacked per frame: a light keeps its tile until it loses it to a
more important light. A quadrant/free-list scheme over a few fixed tile sizes (1024/512/256/128) is
enough; Godot's quadrant atlas is the reference. Importance ≈ screen-space size of the light's
bounding sphere, clamped by distance.

### Wiring the lookup into the forward shader

The pleasing part: **the light UBO entry already has the space.** `LightsData::Light` ends with
`CutOffInner, Pad4, Pad5, Pad6`. Two of those pads become `ShadowViewIndex` (−1 = no shadow) and
`ShadowViewCount` (1 for spot, 6 for point). No layout growth, no std140 re-derivation.

The shadow views themselves go in a new storage — `mat4 + vec4 tile + vec4 params` = 96 bytes per
view. 64 views is 6 KB, which fits a UBO, but note the guaranteed minimum UBO size is 16 KB and
`InstanceData` is already 49 KB, so this is the first thing that genuinely wants the SSBO path
`Graphics::ShaderStorage` has been promising in its own comment since it was written. Either is
fine for v1; say which on purpose rather than by accident.

**The NVIDIA varying-array rule does not bite here, and it looks like it should.** The hand-unrolled
subscripts in `generic.fragment.glsl` exist because `vIn.lightDir[]` is a *varying* array. The
shadow lookup indexes `lights[index]` and `shadowViews[...]`, which are uniform-block arrays —
dynamic indexing of those already happens today (`lights[index].color`) and is fine. So the shadow
code can live inside `CalculatePositionalLight` / `CalculateSpotLight` as a normal loop-free helper,
and `CalculatePointLights` stays exactly as unrolled as it already is. No new unrolling.

## 4. Point lights specifically

Face selection in the shader from `L = worldPosition - light.position`: pick the major axis of
`L` (largest `abs` component) → face 0..5 → `view = shadowViews[light.shadowViewIndex + face]`.
Project, divide, compare. Six `if`s or a `mix` chain; cheap.

The two things that will actually bite:

**Seams.** Bilinear PCF at a face edge samples the neighbouring tile, which is a different face —
a bright/dark line along every cube edge. Standard fixes, both needed: render each face with a
slightly widened FOV so the tile contains a border, and clamp the sample UV to the tile rect inset
by half a texel. This is the tax the atlas charges over a cube array; budget an afternoon of
squinting at a sphere in a room, not five minutes.

**Bias.** The directional path avoids acne with `FaceCullMode::Front`, which works because the
scene is closed-ish and lit from far away. For point lights at short range, front-face culling
peter-pans badly and breaks on any single-sided geometry. The right answer is slope-scaled depth
bias plus normal-offset — and slope-scaled bias means `glPolygonOffset`, which is **not in
`IGraphicsAPI`** today (see §5). Storing linear distance-to-light instead of projected depth makes
the bias much easier to reason about, at the cost of a colour attachment or a manual depth write;
worth considering, not worth blocking on.

**One pass or six?** `gl_ViewportIndex` with a 6-entry viewport array (GL 4.1, ARB_viewport_array)
lets a geometry shader emit all six faces in one batch walk — the atlas equivalent of what
`shadow.geometry.glsl` does with `gl_Layer` today. It trades away per-face culling and pays GS
throughput. Recommendation: **six separate views**, because it keeps the loop uniform with spot and
cascade, and because with caching the six walks mostly do not happen at all. Keep viewport-array in
the back pocket for the case of many *moving* point lights, which is the one case where caching
cannot help.

## 5. Prerequisites

These are ordered by how much they block.

**(1) Lights need a range. This is the real blocker.** `Light` has `(constant, linear, quadratic)`
attenuation and no cutoff, i.e. infinite reach. A shadow view needs a far plane; the atlas needs an
importance metric; caster culling needs a light volume; the fragment shader wants an early-out. All
four are the same missing number. Two options: add an authored `Range` (Unity's model) and derive
the attenuation curve from it, or keep the curve and solve for the radius where contribution drops
below ~1/256. Settled: the explicit field. The curve is a poor authoring surface anyway, and the
derived-radius version silently changes a light's shadow extent whenever someone nudges the
quadratic term. Old levels are not being migrated (§9.3), so `Attenuation` goes away rather than
staying as an override.

**(2) Four additions to `IGraphicsAPI`.** Cheap to write, but each is a hard requirement rather
than a nice-to-have, so they belong in step 1 before anything depends on them:

- **Scissor.** There is `SetViewport` but no scissor, and `glViewport` alone does not restrict
  `glClear`. Without scissor the only way to clear a tile is to clear the whole atlas — which
  destroys every cached tile, i.e. it makes caching impossible. ~10 lines.
- **Depth bias** (`glPolygonOffset`). See §4.
- **Sized depth formats.** `TextureFormat::Depth` translates to the *unsized* `GL_DEPTH_COMPONENT`,
  leaving precision to the driver. Local lights have short ranges and are fine at 16-bit; a 4096²
  atlas is 32 MB at D16 versus 64 MB at D32F. Adding `Depth16` / `Depth32F` is a two-line switch
  entry and halves the memory. (Worth doing for the existing cascade array too — see §8.1, it is
  the single biggest depth allocation in the engine and nobody chose its precision.)
- **Attach a single array layer** (`glFramebufferTextureLayer`). `GLFrameBuffer::AttachTexture`
  takes the layered `glFramebufferTexture` path for any array texture, which is what forces the
  cascades into the geometry shader. Needed to render cascades per view — see §8.3.

**(3) Caching needs an invalidation signal, and the existing one is consumed too early.** A view
must re-render when the light moved/changed, or a caster intersecting its frustum moved, or a
caster was added/removed. Pine already has `Entity::IsDirty()` / `Transform::IsDirty()` — that is
exactly what `SceneLightsProcessing::HasSlotInputChanged` uses. The gotcha:
`SceneProcessor::Prepare` clears every entity's dirty flag at the end of itself (with a `TODO`
acknowledging it should not). Shadow invalidation must therefore run inside `Prepare`, before that
clear, or get its own signal. The cheap correct test is O(dirty objects), not O(objects): keep the
list of renderers whose transform changed this frame and test only those against each view frustum.

The next step after whole-view invalidation — worth naming now, **not** worth building now — is the
static/dynamic split: render static casters into the tile once, keep a copy, and re-composite only
moving casters each frame. It is what makes a swinging lamp in a static room cost one small
re-render instead of a full one. It needs two tiles per view or a depth blit, so it wants to be
designed into the atlas allocator's tile ownership rather than bolted on.

**(4) Directional shadow culling is *not* the same test as camera culling** — relevant because the
obvious first move is "just point `Cull()` at the cascade frustum". For a perspective view (spot,
point face) the light is at the apex, so nothing can be between the light and the near plane and
the plain frustum test is exactly right. For the directional ortho box, casters *behind* the box on
the light side must still be drawn or their shadows vanish. `BuildProjectionMatrix`'s
`farPlaneMargin` half-covers this today by accident. Culling cascades correctly means extending the
frustum's near plane toward the light (a "cast volume", not the view volume) — so spot and point
can use `Cull()` as-is, and cascades need one deliberate extra step.

## 6. Staging

1. **Groundwork, no visible change.** `Range` + `CastShadows` on `Light` with editor fields; the
   four `IGraphicsAPI` additions; array-size `#define` injection (§8.2), which also fixes the live
   `instances[128]` bug.
2. **`ShadowView`, spot lights only, plus cascades expressed as views.** One view per spot; the
   cascades become `ShadowView`s rendering one layer per view into their existing array, with the
   same whole-batch draw as today. This is where the atlas, the tile allocator, the UBO plumbing
   and the shader lookup get proven on the simplest possible view — and a shadowed spot light is
   the single most useful thing for the horror slice anyway, being a flashlight.
3. **Cascade culling with a cast-volume frustum** (§5.4). Its own step because it is the one that
   can regress working directional shadows, and it has no bearing on spot or point.
4. **Caching + importance-driven tile sizes.** Still spot-only. This is where the design either
   holds up or does not, and it is far easier to debug with one view per light.
5. **Point lights: 6 views, face selection, seam handling.** By now this is "allocate six tiles
   instead of one" plus the shader's major-axis pick.
6. **Migrate cascade storage into the atlas**, delete `shadow.geometry.glsl`, the separate
   `Texture2DArray`, and the `Shadows` UBO's cascade-indexed meaning.

Steps 2–5 leave two *storage* paths coexisting (cascade array at sampler 16, atlas at 17) but only
one *mechanism* — everything is a `ShadowView` from step 2 onward. That is the split worth making:
one code path throughout, one temporary fork in the fragment shader's sampler choice. Step 6 closes
it, and it stays cheap precisely because `ShadowView` landed early. See §8.3.

## 7. What this inherits, and what it can't take yet

- **It builds on the 5-point/1-spot object light slot layout**, which is asserted in four
  unconnected places (`Specifications.hpp`, `SceneLightsProcessing.cpp`, `Renderer3D::AddLight`,
  the unrolled subscripts in `generic.fragment.glsl`). Shadows add a *fifth* place that has to
  agree, because "which slot's shadow do I sample" follows the same layout. If that layout is ever
  going to change, changing it before adding shadows is much cheaper than after.
- **The shadow view index rides in `Renderer3D::LightHintData`** (alongside the existing
  `LightIndex`), which means view allocation has to happen before `AddLight` runs. That is
  scene-level work in `Pipeline3D::Prepare()`, which is also where local shadow views should render
  anyway (§1). Both constraints point the same way, which is a good sign.
- **It does not fix the forward-shading light count.** Five point lights per object stays five,
  shadowed or not. Clustered forward is the door this leaves open — a `ShadowView` array indexed by
  light survives that transition unchanged, which is a deliberate reason to put the index in the
  light entry rather than in the per-object instance slots.
- **Terrain and editor gizmos still draw with no light hint data** (noted in `docs/rendering.md`),
  so they will read `Instances[0]`'s arbitrary global light subset — and now its arbitrary shadow
  views with it. Shadows make that existing wrongness more visible, not worse.
- **Unrelated but adjacent, and shadow passes will make it more likely to bite:**
  `MAX_INSTANCE_COUNT` is 512 in C++ while `uniform-buffers.glsl:9` declares `Instance
  instances[128]`. Instances 128–511 read out of bounds in the shader. Shadow passes batch the
  same geometry with a cheaper shader, so they hit large batches sooner than the main pass does.

## 8. The three decisions, in detail

### 8.1 Atlas size and tile budget

The question is usually posed as one number, but it is really two independent ones that get
conflated, and separating them makes both easy:

- **Tile resolution** is a *quality* decision, set by texel density — how much world space one
  shadow texel covers.
- **Tile count** is a *budget* decision, set by how many shadow-casting lights are visible at once.

Atlas size is just their product, so there is no third decision.

**Texel density.** For a spot with outer half-angle θ at range R, the far plane spans
`2·R·tan(θ)` across the tile:

| light | tile | world size per texel |
|---|---|---|
| spot, 45° outer, 10 m range | 1024² | 2.0 cm |
| " | 512² | 3.9 cm |
| " | 256² | 7.8 cm |
| point face (90° FOV), 8 m range | 512² | 3.1 cm |
| " | 256² | 6.3 cm |

For the PSX-styled look this is the rare case where the aesthetic argues for the cheap option: a
crisp 2 cm shadow edge next to affine-mapped low-poly geometry looks *wrong*. 256–512 is not a
compromise here, it is the correct answer. That also means the tile-size decision is much less
load-bearing than it would be in a realistic renderer — you can be wrong by a factor of two and it
will still look right.

**The counter-intuitive part: atlas size costs memory, not frame time.** With scissor + caching you
clear and re-render only invalid tiles. An atlas where nothing moved costs *zero* per frame
regardless of its size. What costs frame time is the number of *dynamic* shadowed lights. So
"make the atlas bigger" is nearly free, and the usual instinct to be frugal here buys nothing.

Worse, being frugal actively hurts: if the atlas is too small the allocator starts evicting, and an
evicted light re-renders the frame it gets its tile back. A light oscillating on the eviction
boundary re-renders *every frame* — the exact worst case caching exists to prevent. **Undersizing
the atlas converts a memory saving into eviction churn.** That is the real argument, and it points
at sizing generously.

**Memory, for scale** (D16 = 2 B/texel): 1024² = 2 MB, 2048² = 8 MB, 4096² = 32 MB. Compare to
what is already allocated: the cascade array is `ShadowMapResolution² × 2` layers at an *unsized*
depth format, so at the `High` preset's 4096 the driver is handing out roughly **130 MB**. An 8 MB
atlas is a rounding error beside it. If VRAM ever becomes the problem, the lever is the cascade
resolution and a sized depth format — not the atlas.

**Recommendation:** 2048² D16 at `High`, 1024² at `Low`/`Medium`, 4096² at `Ultra` — a new
allocation-class field next to `ShadowMapResolution` in `GraphicsSettings`, which already models
"restart to change". Tile classes 512/256/128, with **point lights capped one class below spots**:
a point light eats six tiles, and its shadow is spread over a whole sphere at short range, while
the spot is the flashlight the player stares down. At 2048² that budget holds roughly 8 spots at
512² plus 5 point lights at 256², simultaneously — far more than a room needs.

One thing to build anyway: a debug override forcing a tiny atlas. An allocator that never runs out
is an allocator whose eviction path is never tested, and that bug will otherwise surface in a large
level at the worst possible time.

### 8.2 UBO or SSBO for the shadow view array

I overstated the case for SSBO in the first pass. Checking the actual numbers, most of the
arguments evaporate:

- **Size is a non-issue.** ~96 bytes per view; 64 views = 6 KB, 128 views = 12 KB. Both fit a UBO
  easily. My "16 KB guaranteed minimum" point was weak — `InstanceData` is already 49 KB, so the
  engine has *already* committed to the 64 KB desktop limit. The view array is nowhere near being
  what breaks that.
- **std430 buys no packing here.** The view struct is `mat4 + vec4 + vec4`; everything is already
  16-byte aligned, so std140 and std430 produce byte-identical layouts. std430 wins for arrays of
  scalars and vec2s, which this isn't.
- **Performance mildly favours the UBO.** UBO reads go through the constant cache, and the access
  pattern here is highly coherent — neighbouring fragments hit the same light and the same view.
  SSBO reads go through general memory. The difference is small at this size, but it points toward
  the UBO, not away from it.
- **The cost is not "a flag".** There is no SSBO path at all: `CreateUniformBuffer` is the only
  buffer factory, `GLUniformBuffer::Create` hardcodes `GL_UNIFORM_BUFFER` in three calls, and
  `ShaderStorage<T>` holds an `IUniformBuffer*`. Adding one means a storage-buffer interface, a GL
  implementation, a `glShaderStorageBlockBinding` path in the shader program, and bumping the
  shaders from `#version 420 core` to 430. (The context is GL 4.5, so availability is not the
  issue — only the work is.) That is a new backend introduced *in the middle of* a shadow feature:
  when the lookup comes out wrong, you get to wonder whether it is the maths or the buffer.

That leaves exactly one real argument for SSBO, and it is a good one: **a `buffer` block can be
declared with an unsized array, so the count never has to be duplicated between C++ and GLSL.**
The engine has already lost that game once — `MAX_INSTANCE_COUNT` is 512 in `Specifications.hpp`
while `uniform-buffers.glsl` declares `instances[128]`, and instances 128–511 read out of bounds
today with nothing reporting it. Adding a third hand-synchronised array count to that pile is a
genuinely bad idea.

But the fix for *that* is not SSBOs. `Shader::CompileShader` already injects `#define` lines after
the `#version` directive (that is how `VERSION_DISCARD` and friends work). Injecting
`MAX_INSTANCE_COUNT`, `DYNAMIC_LIGHT_COUNT` and `SHADOW_VIEW_COUNT` from `Specifications.hpp` as
unconditional defines is ~10 lines in that existing path, and it fixes the whole class of bug for
every UBO at once — including the live `instances[128]` one — rather than only for the new array.

**Recommendation: UBO, plus inject the array sizes as compile-time defines.** Do the define
injection first; it is small, it fixes an existing bug, and it removes the only good reason to
reach for the SSBO. Revisit the SSBO backend when a feature genuinely *needs* it rather than merely
tolerating it — clustered forward light lists is the plausible first such caller, and it is a much
better place to build the backend because that feature cannot work without it.

### 8.3 Fold the cascades in now, or later?

I conflated two separable migrations in the first pass, and separating them dissolves most of the
tension:

1. **Data model** — cascades become `ShadowView`s: matrix, frustum, visibility set, render target,
   driven by the shared build → cull → render loop.
2. **Storage** — cascades stop using their own `Texture2DArray` and move into the atlas.

**Essentially all the benefit is in (1).** Per-cascade culling is a real win *today*: the layered
geometry shader duplicates every triangle into every layer, so cascade 0 — which covers 10 metres —
currently redraws everything within ~38.7 m. (1) also deletes `Shadows::RenderScene`, which is a
near-copy of `Pipeline3D::RenderBatch` with its own distance test, in favour of the shared one.

**Essentially all the risk is in (2).** It changes the sampler, the fragment-shader lookup path, the
bias tuning and the depth format, for a feature that currently works. If spot shadows come out
wrong at the same time, you cannot tell which half broke.

So: **do (1) with the spot work, defer (2) to the end.** Cascades become `ShadowView`s immediately
and render one layer per view into the existing array; the fragment shader is untouched, still
sampling `sampler2DArrayShadow` at slot 16 for directional and the new atlas at 17 for locals. The
fragment shader is then the only place that stays split, and it is the cheapest place for that.

This needs one new call — attaching a *single layer* of an array texture
(`glFramebufferTextureLayer`), since `GLFrameBuffer::AttachTexture` currently takes the layered
`glFramebufferTexture` path for any array texture. Trivial, and independently useful.

**It also makes the abstraction better rather than more compromised**, which is the strongest
argument for this ordering. Forcing cascades through `ShadowView` immediately reveals that a view
must name *where it renders* — a (framebuffer, layer, viewport rect) triple — not merely an "atlas
tile". That generalises correctly to a second atlas, an offline bake target, or a reflection
probe's faces, none of which would want a field called `AtlasTile`. Designing that from the atlas
alone, you would get it wrong and only find out at step 5.

**One wrinkle worth naming now:** cascades are a special case for an importance-driven allocator —
they want the same large tiles every frame and must never be evicted, so the allocator needs pinned
reservations. That is a further argument for keeping cascades *out of the atlas* until the
allocator has proven itself on the lights it was actually designed for.

**Sequencing caveat.** Enabling per-view culling for cascades pulls in the cast-volume problem
from §5(4) — for the ortho box, casters behind it on the light side still cast into it, so the
plain view frustum is the wrong test and using it causes shadows to pop in and out. Spot and point
do not have this problem at all. So make it its own step rather than riding along:

> cascades become `ShadowView`s (same whole-batch draw as today, no culling change) → *then*
> enable cascade culling with a cast-volume frustum → *then* migrate storage into the atlas.

That keeps every step's failure mode legible, which given there is no test suite and you are the
one running the editor is worth more than the step count.

---

## 9. Decisions taken, and what they pull in

Settled: `ShadowView` as the unit; Unity-style `Range` on non-directional lights; `CastShadows` /
`ReceiveShadows` flags; 2048² D16 atlas at `High`; a shadow budget of roughly 1 directional +
2 point + 1 spot. This section is what those choices imply that the choices themselves don't say.

### 9.1 The budget is sound — express it in tiles, not in light counts

As tiles: 2 point × 6 faces + 1 spot = 13. At 256² faces and a 512² spot that is 25% of a 2048²
atlas; even at **512² for everything** it is 81%. So the budget is small enough that point faces do
not need to be a size class below spots after all — with only two of them, they can have the same
resolution as the spot. That supersedes the cap suggested in §8.1, which was sized for a much
larger budget.

The budget being this small has a pleasant consequence: **the atlas allocator degenerates into a
fixed slot table.** With 13 tiles of two size classes there is nothing to pack — the interesting
logic moves entirely to *which lights win the slots*. Keep the interface as "request a tile of size
class N, receive a viewport rect", so raising the budget later adds slots rather than a rewrite,
but do not build a general packer for this.

The one number I would change: **make it 2 spots, not 1.** In a first-person horror game the
flashlight is a spot and will hold that slot permanently, so with a cap of 1 no *world* spot — a
hanging lamp, light through a doorway — can ever cast while the torch is on. A second spot tile is
512² = 6% of the atlas.

Expressing the budget in tiles rather than per-type counts also matters for a reason beyond
tidiness: it lets a level trade. A scene with five spots and no point lights should just work,
and per-type counts cannot express that. Per the naming rule, a budget shaped as
"2 point + 1 spot" is a budget shaped around its first caller.

### 9.2 The selection policy is what a small budget makes load-bearing

This is the biggest thing the decision list does not yet cover. With a cap of two shadowed point
lights chosen by importance, *the choice changes as the player walks*, and both failure modes are
visible:

- **Popping.** A light gains a slot and its shadow appears; loses it and the shadow vanishes.
  Needs the shadow *strength* to fade in and out over a few frames rather than switching hard, and
  the fade has to live in the light's UBO entry so the shader multiplies by it — cheap, but it is a
  field that must exist from the start rather than being retrofitted.
- **Thrash.** Two lights either side of the importance boundary swap every frame, and each swap is
  a cold six-face render. This is §8.1's eviction churn arriving through the *budget* instead of
  through the atlas size, and the fix is the same: hysteresis (a challenger must be meaningfully
  more important to displace an incumbent) plus a minimum residency time.

The general point: **the smaller the budget, the more the selection policy matters.** At a cap of
eight this would barely show; at two it is the difference between "shadows work" and "shadows
flicker when you walk". Budget the effort accordingly — this deserves more care than the tile
allocator does.

Related: with two rendering contexts (editor viewport + game camera) sharing one scene-level atlas,
"importance" is ambiguous. Pick the primary rendering context's camera deliberately rather than
whichever ran last.

**The flashlight is the per-frame cost floor.** A camera-attached spot moves every frame, so its
view is invalidated every frame and it never benefits from caching. Everything else in a static
room caches to zero. So the steady-state cost is "one spot re-render per frame", and the budget
should be read as "one always-hot tile plus twelve usually-cold ones".

### 9.3 Range

Old levels are not being migrated, so `Attenuation` can simply be replaced: drop it from the
serializer and the editor rather than keeping a compatibility path. Existing lights will need
re-authoring, and it is worth knowing by how much — with the current defaults `(1, 0.045, 0.0075)`
the curve does not fall below 1/256 until roughly **181 units**. Today's lights are, for practical
purposes, infinite, so a Unity-like default `Range` of 10 will look like a dramatic change when
levels are re-opened. That is expected, not a bug.

Two things that do carry forward:

- **Range is not only a perf knob — it is what makes the shadow far plane valid.** The falloff has
  to actually reach zero at `Range`, or light leaks past the distance where its shadow map ends and
  produces unshadowed illumination beyond the frustum. A windowed inverse-square
  (`sqr(saturate(1 - sqr(d/range))) / (1 + d²)`) is the right shape, and it is also the one that
  fits the HDR/physical-intensity work already landed.
- **`SetRange` must raise the entity dirty flag,** exactly as `SetLightType` already does — range
  feeds both slot selection and shadow-view invalidation. Easy to miss; the pattern is established.

**A bonus that falls out for free:** `Frustum::Intersects(center, radius)` exists today and has
**no callers**. Range gives every non-directional light a bounding sphere, so
`CollectWorldLights`'s standing `TODO: Add checks to check if this light is relevant` becomes a
two-line test against the camera frustum, using a function that is already written and already
commented. Lights outside the view stop consuming light-buffer slots and stop competing for shadow
tiles.

### 9.4 The flags, and where they actually live

- **`CastShadows` on `Light`** — feeds budget selection; an explicit opt-out beats any heuristic.
- **`CastShadows` on `ModelRenderer`** — likely the bigger performance win of the two, since it
  removes an object from *every* shadow view's draw. But it needs care in `RenderCulling::Cull`,
  whose whole design point is that it knows nothing about shadows. Do **not** teach it about
  casters. The shape that stays generic: give `Cull` an optional `const VisibilitySet* restrictTo`
  meaning "only consider objects in this set", and have `SceneProcessor` maintain a caster set once
  per frame. That parameter reads as "cull a subset", not "cull for shadows" — and it pays for
  itself twice, because it is also how a point light culls once against its bounding sphere and
  then restricts the six per-face culls to those candidates.
- **`ReceiveShadows` on `ModelRenderer`** — needs to reach the fragment shader per object.
  `Instance` carries `ivec4 lightIndices[2]` = 8 ints with **6 used**, so a per-object flags int
  fits in the existing padding with no std140 change. The branch is per-object and therefore
  perfectly coherent across a draw; effectively free.
- **Terrain: deferred, deliberately.** `TerrainRendererComponent` renders through its own path,
  already has no light hint data (see `docs/rendering.md`), and both casts and receives shadows —
  so it will eventually want these flags plus a `ShadowView`-aware draw. Explicitly out of scope
  here. Flagged because it is the second caller that will want the flags to live somewhere shared
  rather than on `ModelRenderer` specifically, and knowing that in advance is the difference
  between moving them later and duplicating them later.

### 9.5 Two traps worth knowing before writing code

**D16 makes the shadow near plane a real decision.** Perspective depth precision is dominated by
the near/far ratio, and 16 bits is not much to spend. For a light with `Range` 10 m: a 0.1 m near
plane gives roughly 1.5 cm of depth resolution at the far plane, which normal-offset bias absorbs
comfortably; a 0.01 m near plane gives ~15 cm, which will produce acne no bias can fix. So derive
the near plane from the range (`max(0.05, Range * 0.005)` or similar) rather than copying the
camera's 0.01. If acne shows up anyway, D24 costs 4 MB more on a 2048² atlas — against a ~130 MB
cascade array, that is not a number worth defending.

**Production mode updates transforms late.** In `m_ProductionMode`, `Transform::OnRender` is not
called by `RenderManager`; it runs inside `RenderBatch`. `Shadows::RenderScene` compensates by
calling `OnRender` per object itself. When shadow rendering moves onto the shared
`Pipeline3D::RenderBatch`, that compensation has to survive, or GameHost shadows will silently use
last frame's matrices while the editor looks fine — the worst possible split, since the editor is
where you would test it.

### 9.6 One thing to build early: an atlas visualiser

A debug panel that draws the atlas with tile borders and labels each tile with its owning light,
size class, and whether it re-rendered this frame. Tile allocation, eviction thrash, cube seams and
stale-cache bugs are all invisible in the final image and obvious in that view. The `DebugPanel`
already grew culling statistics; this is the same move, and it will pay for itself during §9.2 in
particular.

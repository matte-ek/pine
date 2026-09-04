# Implementation plan — spot & point light shadows

Companion to [`spot-point-light-shadows.md`](spot-point-light-shadows.md). That report argues *what*
shape to build and why; this is the ordered build, verified against the working tree at
`ai` @ 6994881 + uncommitted lighting/culling work (2026-09-04).

Seven steps instead of the report's six: its step 2 bundled "cascades become views" with "spot
lights get shadows", and those have opposite risk profiles — the first must produce a *pixel-identical*
image, the second is the first new visible thing. Splitting them is what makes each step's failure
mode legible, which matters more here than step count because there is no test suite and you are the
one running the editor.

Every step ends with something you can look at and judge. No step leaves the tree in a state where
directional shadows are broken "until the next one".

---

## What the code says that the report didn't know

Six things I found while reading the tree that change the plan rather than just decorating it.

**1. `Pipeline3D::RenderBatch` renders terrain, so shadows can't share it as-is.** The first thing
`RenderBatch` does in `Opaque` mode is loop every `TerrainRendererComponent` and call
`TerrainRenderer::Render`, which uses its own shader and its own camera state — it would ignore
`OverrideShader` and draw a lit terrain into the depth atlas. That block has to move up into
`RenderScene` before shadows can call `RenderBatch`. It doesn't belong there anyway: `RenderBatch`
is named for the batch it draws, and the terrain isn't in it. Small change, but it's a hard
prerequisite for deleting `Shadows::RenderScene`, so it goes in step 2.

**2. The shadow pass doesn't need a new uniform for its matrix — `Renderer3D::SetCamera(view, proj)`
already exists** and writes the `Matrices` UBO. So a `ShadowView` renders by calling that overload,
and `shadow.vertex.glsl` becomes the same shape as `depth.vertex.glsl`:
`gl_Position = projectionMatrix * viewMatrix * transformationMatrix * vertexPosition`. The
`Shadows` UBO then disappears from the shadow shader entirely — its cascade-indexed meaning survives
only in the fragment path, which is exactly where step 7 deletes it. This is nicer than it sounds:
it means the shadow shader stops being special, and the geometry shader goes away with no
replacement.

**3. Deleting `shadow.geometry.glsl` requires `EngineCli --import` on an asset that already has a
`.passet`, and that is safe *for this one asset*.** `Asset::ReImport` iterates the asset's own
`m_SourceFiles` (read from the `.passet`), not the `.ih` — so HotReload can never *remove* a source
file. Re-importing mints a new UId, which is normally how you break every material pointing at a
shader. It's fine here, verified two ways: the shadow shader is fetched by path only
(`Assets::Get<Shader>("engine/shaders/3d/shadow")` in `Shadows.cpp:241` is the sole reference), and
its `.ih` declares no `Versions` to lose. The hard-coded UId in `Material.hpp:31` is the *generic*
shader, untouched. This is a deliberate, one-time exception to the standing rule in CLAUDE.md, and
it should be called out in the commit message so nobody reads it as precedent.

**4. Sampler bindings come from the `#shader bind` directive in the source, not the `.ih`.**
`ShaderImporter::ProcessShaderLine` parses them and `Import` clears the list first, so adding
`#shader bind ShadowAtlas 17` to `shadows.glsl` and touching `generic.fragment.glsl` is enough —
the atlas needs no re-import of the generic shader, and no UId churn. (The `.ih`'s `TextureSamplers`
block is redundant for reimport; keep it in sync anyway so a future `--import` of a *new* shader
isn't surprising.)

**5. Light buffer index 0 is overloaded, and the shadow index must not repeat the mistake.**
`Renderer3D::UploadLights` resets `m_CurrentLightIndex = 1`, directional always takes slot 0, and
`Instance.LightIndices[i] = 0` is simultaneously "the directional light" and "this slot is empty" —
which is why the fragment shader tests `if (vIn.lightIndices[0] != 0)`. It works only because a
local light can never land at 0. Use `-1` for "no shadow view", never 0, and don't let the two
sentinels drift into each other.

**6. `RenderCulling::Cull` is O(all ModelRenderers) *and* clears `m_MaxObjectCount` bits per call.**
At the step-6 budget that is 13 full walks per frame. The report's `restrictTo` parameter isn't a
tidiness nicety, it's what makes per-face culling cheaper than not culling. It arrives in step 6.

The obvious cheaper half — pre-culling lights by their bounding sphere against the camera frustum,
using the already-written-and-uncalled `Frustum::Intersects(center, radius)` — looked free and is
not; see 1f, where it was tried and backed out.

One thing the report worried about that turns out to be already handled:
`Pipeline3D::RenderBatch` does call `Transform::OnRender` per object, so the production-mode
late-transform compensation in `Shadows::RenderScene` survives the move automatically. No action.

---

## Step 1 — Groundwork — DONE

No shadow behaviour changes. Everything here is a prerequisite that something later depends on, and
each piece is independently useful.

### 1a. `Light` gains `Range`, loses `Attenuation`

`World/Components/Light/Light.hpp` / `.cpp`:

- `float m_Range = 10.f;` with `SetRange`/`GetRange`. **`SetRange` must call
  `GetParent()->SetDirty(true)`**, same as `SetLightType` already does — range feeds slot selection
  and, later, shadow-view invalidation.
- Delete `m_LightAttenuation` and its accessors. Drop `PINE_SERIALIZE_PRIMITIVE(Attenuation, …)`,
  add `PINE_SERIALIZE_PRIMITIVE(Range, Float32)`. No compatibility path — levels are not being
  migrated (report §9.3).
- `bool m_CastShadows = true;` + serializer field. The report said opt-*in*; I'd default it on. Its
  argument was cost, and the budget plus importance selection (step 5) is what actually bounds cost —
  the flag's real job is letting an author mark a light as cheap fill. Defaulting on also means the
  directional light keeps casting with no per-level migration, which keeps step 1's "nothing visible
  changes" contract intact. One character to flip if you disagree.

Removing `Attenuation` is fully contained — six sites (`Light.hpp/.cpp`, `ShaderStorages.hpp`,
`Renderer3D.cpp:437`, the editor panel, `common.glsl`, `lightning.glsl`). Nothing in `ScriptRuntime/`
touches it; C# only knows `Light` as a `ComponentType` enum entry.

`Editor/.../ComponentPropertiesRenderer.cpp` `RenderLight` (~line 157): the three attenuation
sliders go, one `Range` input and a `Cast Shadows` checkbox arrive. Show `Range` only for
point/spot — a directional light has no range and showing one is a lie.

**Expect existing levels to look dramatically different.** With the old defaults `(1, 0.045, 0.0075)`
the falloff doesn't reach 1/256 until ~181 units, so today's lights are effectively infinite; a
default `Range` of 10 is not a regression, it's the first time the number was honest. Re-authoring
is expected.

### 1b. The falloff curve

`shared/lightning/lightning.glsl`, `CalculatePositionalLight`. Replace the
constant/linear/quadratic reciprocal with a windowed inverse-square:

```glsl
float d2 = dot(toLight, toLight);
float w  = clamp(1.0 - d2 / (range * range), 0.0, 1.0);
float attenuation = (w * w) / (1.0 + d2);
```

This is not a cosmetic swap. **The falloff must actually reach zero at `Range`, or light leaks past
the distance where its shadow map ends** and you get unshadowed illumination outside the shadow
frustum — a bug that would look like a shadow bug and isn't. It also fits the physical-intensity
model the HDR work already landed.

### 1c. Light UBO field repurpose

`ShaderStorages.hpp`, `LightsData::Light` — **repurpose in place, do not re-derive the layout.**
The struct is five `vec4`s; the fields exist:

| was | becomes |
|---|---|
| `Vector3f Attenuation` | `float Range; float Pad2a; float Pad2b;` |
| `float Pad4` | `int ShadowViewIndex` (−1 = none) |
| `float Pad5` | `int ShadowViewCount` (1 spot, 6 point) |
| `float Pad6` | `float ShadowFade` (0..1, for step 5's fade-in) |

Mirror in the GLSL `Light` struct in `shared/common.glsl`. `ShadowFade` is dead weight until step 5
and belongs here anyway — retrofitting a field the shader must multiply by, after the shader
already works, is how you end up multiplying in three places.

A tidier four-`vec4` layout is available (`vec3 position; float range;` and so on, 64 bytes instead
of 80). Not worth it now: it re-derives std140 for the one buffer that currently works, in the step
whose whole point is that nothing visible changes. Revisit when something else forces the struct
open.

### 1d. Four `IGraphicsAPI` additions

All small, all hard requirements, all independently sane to have.

- `SetScissorEnabled(bool)` / `SetScissor(Vector2i pos, Vector2i size)`. **Without scissor there is
  no way to clear one atlas tile**, only the whole atlas — which wipes every cached tile, i.e. it
  makes step 5 impossible. `glViewport` alone does not restrict `glClear`.
- `SetDepthBias(float constant, float slope)` + enable/disable → `glPolygonOffset`. Slope-scaled
  bias is what replaces front-face culling for short-range perspective views (step 3).
- `TextureFormat::Depth16` / `Depth32F`. `TextureFormat::Depth` maps to the *unsized*
  `GL_DEPTH_COMPONENT` (`GLTexture.cpp:115`), so the driver picks. Two switch entries. Worth doing
  for the cascade array too — at the `High` preset's 4096² × 2 layers that is the single largest
  depth allocation in the engine and nobody chose its precision.
- `IFrameBuffer::AttachTextureLayer(ITexture*, BufferAttachment, int layer)` →
  `glFramebufferTextureLayer`. `GLFrameBuffer::AttachTexture` takes the layered
  `glFramebufferTexture` path for anything with `GetArraySize() > 0`
  (`GLFrameBuffer.cpp:226`), which is precisely what forces cascades through a geometry shader.

### 1e. Inject array sizes as shader defines

`Shader::CompileShader` already inserts `#define` lines after the `#version` line for versions
(`Shader.cpp:34-45`). Add unconditional defines from `Specifications.hpp`:
`MAX_INSTANCE_COUNT`, `DYNAMIC_LIGHT_COUNT`, `SHADOW_VIEW_COUNT`. Then
`shared/uniform-buffers.glsl` uses them.

**This fixes a live bug**: `MAX_INSTANCE_COUNT` is 512 in C++ while `uniform-buffers.glsl:9`
declares `Instance instances[128]`, so instances 128–511 read out of bounds today with nothing
reporting it. Shadow passes batch the same geometry with a cheaper shader and hit large batches
sooner than the main pass, so this would have started biting in step 3 regardless.

It is also the reason the shadow view array can be a UBO (report §8.2): the only good argument for
an SSBO was that `buffer` blocks take unsized arrays and can't drift out of sync with C++. Fixing
the drift for *every* UBO at once, in ~10 lines of an existing code path, is strictly better than
introducing a storage-buffer backend in the middle of a shadow feature.

Editing `uniform-buffers.glsl` is a shared `#include`, so **HotReload will not fire on its own** —
also touch `generic.fragment.glsl`, `depth.vertex.glsl` and `shadow.vertex.glsl` (the three
top-level sources that pull in `common.glsl`), then commit the rewritten `.passet`s.

Three things found while checking this, all of which widen the step slightly:

- **There is a second hardcoded `instances[128]`**, in
  `data/editor/shaders/generic-solid/generic-solid.vertex.glsl:20`, with its own copy of the
  `Instance` struct. It cannot include `shared/common.glsl`: `ShaderImporter::ProcessShaderLine`
  resolves includes relative to the shader's own directory, so from `editor/shaders/generic-solid/`
  the path would be `editor/shaders/shared/common.glsl`, which doesn't exist. So it keeps its
  duplicate struct and just picks up the injected define — but it is a **separate asset** and needs
  its own HotReload touch and its own committed `.passet`.
- **`uniform-buffers.glsl:15` has a stray `vec3 blah;`** after `Light lights[32];`. `LightsData` has
  no matching member, so the GLSL block is 2576 bytes against a 2560-byte buffer. Nothing reads it,
  but it is the same class of drift this step exists to kill. Delete it while the block is open.
- **The instance block declaration goes from 12 KB to 48 KB.** No new allocation —
  `ShaderStorage::Create` already sizes the buffer at `sizeof(InstanceData)` = 512 × 96 = 48 KB, so
  the shader has simply been declaring less than exists. Measured on the dev GPU (RTX 3070, driver
  610.57.04, GL 4.5 core, via a headless EGL device context): `GL_MAX_UNIFORM_BLOCK_SIZE` = **65536**,
  so 48 KB fits at 75% and this step's only failure mode is closed. If it ever doesn't on some other
  machine, the fallback is lowering `MAX_INSTANCE_COUNT` to 128 rather than raising the shader —
  the *other* correct fix for the same bug.

### Measured GL limits, and the one that actually binds

Same query, because two of these change how later steps should think:

| limit | value | relevance |
|---|---|---|
| `MAX_UNIFORM_BLOCK_SIZE` | 65536 | 48 KB instance block fits; nothing else is close |
| `MAX_UNIFORM_BUFFER_BINDINGS` | 84 | binding 7 for `ShadowViews` is a non-issue |
| **`MAX_VERTEX/FRAGMENT_UNIFORM_BLOCKS`** | **14** | **the real ceiling — see below** |
| `MAX_SHADER_STORAGE_BLOCK_SIZE` | 2147483647 | the SSBO escape hatch is unbounded |
| `MAX_ARRAY_TEXTURE_LAYERS` | 2048 | cascade array has room to spare |
| `MAX_TEXTURE_SIZE` | 32768 | a 4096² atlas is nothing |

**14 uniform blocks per shader stage is the limit worth remembering, not 64 KB.** The 84 is total
binding *points*; 14 is how many distinct blocks one stage may reference. Pine declares 7 today
(Matrices, Instances, Material, Lights, Shadows, World, AO_DATA); step 3's `ShadowViews` makes 8.
That leaves 6, not 77. Nothing to act on in this plan — step 7 gives one back by deleting `Shadows` —
but it means "add another UBO" is a decision with a countable budget, and the next feature that
reaches for one should know the count.

The 2 GB SSBO size reinforces rather than changes §8.2: the escape hatch is real and unbounded, so
there is still no reason to build it before something needs it. The instance buffer is now a named
candidate for that alongside clustered forward — it is at 75% of its ceiling, and 680 instances is
where the UBO runs out.

### 1f. Cull lights by range — attempted, backed out

This was listed as a free win: `Range` gives every non-directional light a bounding sphere,
`Frustum::Intersects(center, radius)` is already written with zero callers, and
`CollectWorldLights` has a standing `TODO: Add checks to check if this light is relevant`.

**It is not free, and it is now a comment explaining why rather than code.** Two reasons found on
implementing it:

- `CollectWorldLights` feeds `ProcessModelRenderer`, whose per-object `LightSlotIndex` cache is
  **scene-level state shared by every rendering context**. Filtering the light set by one camera's
  frustum makes that shared cache depend on one view, so a light visible in the editor viewport but
  not to the game camera would be dropped from every object's slots in *both* contexts.
- And it would be the wrong camera anyway: `Editor/src/Rendering/RenderHandler.cpp:76` sets the
  primary rendering context to the **game** context. In the editor, "primary" is not the viewport
  being looked at.

Filtering later, at `Renderer3D::AddLight` (which genuinely is per-context), doesn't work either:
`ProcessModelRenderer` assigns object slots from the same light list that `AddLight` assigns buffer
indices from, so skipping a light at upload silently desynchronises the two and objects point at the
wrong light.

Doing it properly means a per-context light set with per-context slot caches, or culling against the
union of active frustums. Neither is a prerequisite for shadows, so it stays out.

**This lands earlier than expected on §9.2's ambiguity.** That section flagged that "importance" is
ambiguous with two contexts sharing one scene-level atlas, and said to pick the primary context
deliberately. The finding here sharpens it: the primary context is the *game* view, so for shadow
tile selection in step 5, `GetPrimaryRenderingContext()` is the wrong default in the editor and
picking it "deliberately" means picking something else — most likely the context currently being
rendered, with the atlas becoming per-context state or the selection being explicitly game-view
authoritative and accepted as approximate in the viewport.

Also added, outside the plan: `OpenGL::Setup` logs the device's uniform block size, binding count,
per-stage block count, shader storage block size and fragment texture units, and errors if the
instance block exceeds what the device supports (naming the constant to lower). The limits that
constrain this work were previously invisible.

**Status: built, all four targets link. Not yet run.**

**Verify:** lights fall off at their range; the scene is darker and needs re-authoring; directional
shadows unchanged; no shader compile warnings about the new defines; the GPU limits line appears in
the log at startup.

---

## Step 2 — `ShadowView`, and cascades expressed as views — DONE

The mechanism step. **The success criterion is that the image does not change.**

### 2a. The type

`Engine/src/Pine/Rendering/ShadowView/ShadowView.hpp` — top-level `Rendering/`, deliberately *not*
under `Features/Shadows/`. A reflection probe's six faces or an offline bake target wants this
struct and should not have to include a shadow feature header to get it.

```cpp
struct ShadowView
{
    Matrix4f ViewProjection;
    Frustum  ViewFrustum;

    // Where it renders: a (framebuffer, layer, rect) triple, not an "atlas tile". Cascades render
    // into a layer of their own array; the atlas is one framebuffer with many rects. Both are this.
    Graphics::IFrameBuffer* Target = nullptr;
    int      TargetLayer = -1;
    Vector4i Viewport = Vector4i(0);

    float DepthBias = 0.f;
    float NormalBias = 0.f;

    Rendering::RenderCulling::VisibilitySet Visibility;
};
```

Forcing cascades through this *before* the atlas exists is what produces the (framebuffer, layer,
rect) triple rather than an `AtlasTile` field. Designing it from the atlas alone you would get it
wrong and find out in step 6.

The bias pair is the only field that reads as shadow-specific, which is the right amount of
compromise for a type named `ShadowView`. If a probe ever wants it, the fields are ignorable.

**Own the view array persistently** (`std::vector<ShadowView>` in the `Shadows` anonymous
namespace), not rebuilt per frame — `VisibilitySet` holds a 512-byte allocation each at the default
4096 objects, and step 5's caching needs the identity to survive frames anyway.

### 2b. Move terrain out of `RenderBatch`

Per finding 1. `Pipeline3D::RenderBatch`'s opening terrain loop moves into `RenderScene`. Nothing
else changes.

### 2c. Delete `Shadows::RenderScene`

It is a near-copy of `RenderBatch` with its own distance test. The shared one takes
`(batch, mode, visibility)` already. Drive it with `Renderer3D::SetCamera(view, proj)` per finding 2.

The per-object `MAX_SHADOW_DISTANCE` test goes away with it — note the name lies (it is 1500 used
*squared*, i.e. ~38.7 world units, and separately under a `sqrtf` for the cascade far plane; both
agree, the name doesn't). Its job is subsumed by the cascade frustum in step 4. Renaming it to
`MAX_SHADOW_DISTANCE_SQR` now costs nothing and stops the next reader — including a local-light
distance fade — getting it wrong.

### 2d. Cascades as views, geometry shader deleted

`BuildLightSpaceMatrices` becomes `BuildCascadeViews`: same maths, but it fills two `ShadowView`s
whose `Target` is the existing depth array framebuffer and whose `TargetLayer` is 0 and 1. Render
loop:

```
for (view : views)
    AttachTextureLayer(depthArray, Depth, view.TargetLayer)
    SetViewport(view.Viewport); ClearBuffers(DepthBuffer)
    Renderer3D::SetCamera(view.View, view.Projection)
    RenderBatch(batch, Opaque, view.Visibility)
```

**No culling change in this step** — every view gets a fully-set `VisibilitySet` and draws the whole
batch, exactly as the geometry shader did. That keeps this step's diff to "same triangles, different
plumbing".

`shadow.vertex.glsl` gains the `Matrices` multiply, `shadow.geometry.glsl` is deleted, `shadow.ih`
loses it from `SourceFiles`, and the asset is re-imported from `data/`:

```bash
EngineCli --import engine/shaders/3d/shadow shadow/shadow.vertex.glsl shadow/shadow.fragment.glsl
```

New UId, safe per finding 3. Commit the new `.passet` + `.ih`. Say so in the commit message.

The `Shadows` UBO (`mat4 lightSpaceMatrix[8]`) is untouched and still cascade-indexed — the fragment
shader still reads it exactly as today. Step 7 is what kills it.

**Status: built, all four targets link. Not yet run.**

Three things found while implementing:

- **Terrain had to be extracted from `RenderBatch` into a `RenderTerrain()` helper called by *both*
  existing callers, not just `RenderScene`.** The depth pre-pass also called `RenderBatch(Opaque)`,
  so it was drawing terrain too — through the terrain's own shader, ignoring the pre-pass's
  `OverrideShader`. Moving terrain to `RenderScene` alone would have silently dropped it from the
  depth/normal buffer and changed ambient occlusion.
- **The shadow pass needs `RenderBatch` twice, Opaque *and* Discard.** The old `Shadows::RenderScene`
  ignored material rendering mode entirely, so alpha-tested geometry cast a solid shadow.
  `RenderBatch` filters by mode, so a single Opaque call would have silently stopped foliage casting.
- **`Pipeline3D.hpp` including `SceneProcessor.hpp` created an include cycle**, since
  `SceneProcessor.hpp` included `Pipeline3D.hpp`. Broken by pointing `SceneProcessor.hpp` at
  `Material.hpp` plus forward declarations — the dependency only ever made sense one way, since the
  pipeline consumes the scene processor's output and not the reverse.

Also: the cascade `MAX_SHADOW_DISTANCE` test was **kept**, moved into `BuildCascadeVisibility` as the
predicate that fills each view's `VisibilitySet`. The plan said to delete it here; that would have
changed the image (objects between ~38.7 and ~43.7 units would start casting into cascade 1). Moving
the predicate rather than removing it puts the mechanism in place while step 4 remains the step that
changes what is drawn.

One cosmetic consequence: the shadow shader no longer references the `Shadows` UBO, so
`Renderer3D::SetShader` logs one new "missing 'Shadows' shader storage" warning and one fewer for
`Matrix`. Pre-existing noise — that shader has always failed to declare most blocks — just
reshuffled.

**Verify:** directional shadows pixel-identical. If anything differs, it is this step, and there is
only one thing it could be. Watch specifically for: shadows disappearing entirely (framebuffer
incomplete after the switch to single-layer attachment), alpha-tested foliage no longer casting,
and terrain missing from ambient occlusion.

---

## Step 3 — Atlas, spot light shadows, and the debug view — DONE

The first new visible feature, and the first thing that is actually useful for the horror slice —
a shadow-casting flashlight.

### 3a. `ShadowAtlas`

`Rendering/Features/Shadows/ShadowAtlas/`. **Do not build a packer.** Report §9.1 is right that at
the target budget (13 tiles, two size classes) there is nothing to pack; the interesting logic is
entirely *which lights win slots*, which is step 5. A fixed slot table is enough. Keep the interface
general enough that raising the budget adds slots rather than forcing a rewrite:

```cpp
int  Acquire(SizeClass, std::uint64_t ownerKey);  // stable across frames; -1 when full
void Release(int slot);
Vector4i GetViewport(int slot);
Vector4f GetUvRect(int slot);
```

2048² `Depth16` at `High`, 1024² at `Low`/`Medium`, 4096² at `Ultra` — a new allocation-class field
next to `ShadowMapResolution` in `GraphicsSettings::Settings`, which already models "restart to
change". Tile classes 512/256/128.

Sizing generously is the right instinct here and it is counter-intuitive enough to state plainly:
**with scissor + caching, atlas size costs memory, not frame time.** An atlas where nothing moved
costs zero per frame regardless of size. Undersizing converts a memory saving into eviction churn,
which is the one thing caching exists to prevent. 8 MB next to the cascade array's ~130 MB is a
rounding error.

Build the debug override that forces a tiny atlas **now**, not in step 5. An allocator that never
runs out is an allocator whose eviction path has never executed.

### 3b. The atlas debug panel

Report §9.6 says build it early; I'd go further and say build it *in this step*, because tile
allocation, cube seams and stale-cache bugs are all invisible in the final image and obvious here.
`DebugPanel` already grew culling statistics, so this is the same move: draw the atlas texture with
tile borders, label each with owning light, size class, and whether it re-rendered this frame. The
last column is dead until step 5 and is the whole point of the panel from step 5 onward.

### 3c. Shadow view UBO

New `ShaderStorages::ShadowViews` at binding 7 (`AO_DATA` is 6). Per view: `mat4 viewProjection;
vec4 tile; vec4 params;` = 96 bytes. At `SHADOW_VIEW_COUNT = 32` that is 3 KB — comfortably a UBO,
and step 1e means the count is defined once in `Specifications.hpp` and injected.

Keep it a *separate* storage from the existing `Shadows` block rather than extending it. Extending
would mean editing the block the (now deleted) geometry shader used to declare, and would tangle
step 7's deletion with step 3's addition.

### 3d. Shader lookup

`shared/lightning/shadows.glsl` gains, alongside the existing cascade path:

```glsl
uniform sampler2DShadow ShadowAtlas;
#shader bind ShadowAtlas 17

float SampleLocalShadow(int lightIndex, vec3 worldPosition, vec3 normal);
```

Called from `CalculateSpotLight`. **One `sampler2DShadow` fetch with `GL_LINEAR` +
`GL_COMPARE_REF_TO_TEXTURE` is already 2×2 hardware PCF** — that is the right default for local
lights, not the directional path's 3×3 kernel. With up to 7 shadowed slots per fragment, the filter
width is the wall (report §2a); make the quality knob a shader version or graphics setting, never a
per-light field, so the cost is bounded by the preset rather than by the level author.

**The NVIDIA varying-array rule does not apply here even though it looks like it should.** The
hand-unrolled subscripts in `generic.fragment.glsl` exist because `vIn.lightDir[]` is a *varying*
array. `lights[index]` and `shadowViews[...]` are uniform-block arrays, and `lights[index].color` is
dynamically indexed today and fine. So no new unrolling, and `CalculatePointLights` stays exactly as
unrolled as it already is.

### 3e. Where local views render

`Pipeline3D::Prepare()`, not `Run(Prepass)`. **A spot or point shadow map does not depend on the
viewer at all**, so rendering it per rendering context — editor viewport *and* game camera — is pure
duplicated work. Cascades stay in `Prepass` because they genuinely are per-camera.

Two consequences to handle rather than discover:

- `Prepare()` has no `RenderingContext`, so shadow draw calls have nowhere to count themselves.
  `Renderer3D::RenderMeshInstanced` already null-checks `m_RenderingContext`, so it works — but the
  statistics silently vanish. Either attribute them to `GetPrimaryRenderingContext()` or add a
  scene-level counter; pick deliberately, don't let it be an accident.
- View allocation must run before `Renderer3D::AddLight` (which is where `LightHintData` is read),
  and `Prepare()` is before every context's `Run`. Both constraints point the same way.

### 3f. Bias

Front-face culling — the directional path's acne fix — **peter-pans badly at short range and breaks
on single-sided geometry**. For local lights use slope-scaled depth bias (1d) plus normal-offset.
The near plane is a real decision at D16: derive it from range (`max(0.05, Range * 0.005)`) rather
than copying the camera's 0.01. At `Range` 10, a 0.1 m near plane gives ~1.5 cm depth resolution at
the far plane, which normal-offset absorbs; a 0.01 m near plane gives ~15 cm, which no bias fixes.
If acne persists, D24 costs 4 MB more on a 2048² atlas and is not a number worth defending.

**Status: built, all four targets link. Not yet run.**

What landed, and where it differs from the sketch above:

- **Atlas** is Godot's quadrant scheme: four quadrants, two subdivided into Large tiles (atlas/4),
  one into Medium (atlas/8), one into Small (atlas/16) — 8 / 16 / 64 tiles. `Acquire` returns the
  slot an owner already holds, and `EndFrame` sweeps anything not re-claimed, so a tile is stable
  for exactly as long as its light keeps asking for it. `SetDebugSlotLimit` forces exhaustion.
- **`ShadowAtlasResolution`** is a new allocation-class graphics setting: 1024 Low/Medium,
  2048 High, 4096 Ultra. Depth16.
- **Selection is first-come-first-served.** Deliberately dumb, and flagged in the code as the piece
  step 5 has to replace — with nothing to make churn visible yet, a smarter policy would be
  untestable.
- **Spot views use back-face culling plus slope-scaled depth bias and a shader-side normal offset**,
  not the cascades' front-face culling. Near plane is derived from range (`max(0.05, range*0.005)`),
  and the cone FOV is widened 4° so the tile carries a border for the PCF tap.
- **Spot shadows attenuate specular as well as diffuse**, unlike the directional path which only
  shadows diffuse. A specular highlight surviving inside a shadow reads as a leak.
- **A `worldNormal` varying had to be added to the generic shader.** `normalDir` is rotated into
  tangent space for normal-mapped materials, so it cannot be used to offset a world position.
- **`ShadowFade` is uploaded but pinned to 1.0** — the field exists and the shader multiplies by it,
  so step 5's fade is a value change rather than a shader change.

**Verify:** a spot light casts a shadow; the atlas panel shows one tile with the right owner;
no acne, no peter-panning; directional shadows still correct; two viewports don't fight. Then set
"Force tile limit" to 0 (all spot shadows should vanish) and to 1 with two spots (the loser should
degrade to unshadowed, not corrupt the winner's tile).

The tile limit shipped broken and was caught on first run. Three bugs compounded into a control that
could not affect anything in a typical scene, and the shape of the mistake is worth keeping:

- **0 meant "disabled" and was also the slider's minimum**, so the lowest reachable cap was 1 — which
  a single-light scene satisfies anyway. Now -1 is off and 0 is a real cap, which also makes 0 the
  setting that proves at a glance the control is connected.
- **The incumbency shortcut returned before the cap was tested**, so a light already holding a tile
  was exempt. The cap could only ever deny newcomers, i.e. do nothing in a settled scene. The check
  now runs first, so lowering the cap evicts.
- **The count scanned owned slots**, but owners are only swept in `EndFrame` — during `Acquire` the
  slots still carry last frame's owners. It now counts claims made this frame.

Each was individually plausible; together they made a debug control that silently did nothing, which
is the one thing a debug control must never do. `MarkRendered` also moved from `PrepareLocalViews`
to `RenderLocalViews`, where the render actually happens — it would have become a lie as soon as
cached tiles stopped re-rendering.

---

## Step 4 — Cascade culling with a cast-volume frustum — DONE

Own step because it is the only one that can regress *working* directional shadows, and it has no
bearing on spot or point.

The plan as written was to keep the projection alone, build a widened "cast volume" frustum
alongside it, and cull cascades against that. **That would have been decorative.** Anything outside
the projection's near plane is clipped by the rasterizer whether or not culling keeps it, so a
culling volume wider than the projection only submits draws that produce no depth. The near plane
that matters is the *projection's*, not the culler's.

### 4a. The sign bug underneath it

`BuildProjectionMatrix` passed light-space z values straight into `glm::ortho`, which takes
`zNear`/`zFar` as positive distances *along* the view direction — while everything the view can see
sits at negative z. The call was `ortho(..., minZ - margin, maxZ + margin)`; the correct one is
`ortho(..., -maxZ - margin, -minZ + margin)`.

It produced a usable box anyway, for a reason worth writing down: `BuildViewMatrix` puts the light
eye exactly **one unit** from the box centre (`center - lightDirection`, and the direction is
normalized), so the box corners straddle the eye and `maxZ` comes out positive. Working through it,
the visible range lands at `[1 - r - m, 1 + r + m]` where the box occupies `[-1 - r, -1 + r]` — it
contains the box as long as `m >= 2`, and `farPlaneMargin` is `farPlane[i] * 0.5`, so 5 and ~21.7.

The accident is exactly the "half-covers this" the plan noticed: it handed the near plane
`2 + farPlaneMargin` of unasked-for slack, which is the only reason casters behind the box cast at
all today — and why cascade 1 got roughly three times as much of it as cascade 0. It also wasted
`2 * farPlaneMargin` of depth range at both ends.

### 4b. Fitting the near plane to actual casters

With the sign fixed, the near plane needs a real value. `FitCasterNearZ` scans `ModelRenderer`
bounds, keeps those whose light-space XY overlaps the cascade box's column, and takes the furthest
`z` any of them reaches toward the light. The XY test is what stops one tall object on the far side
of the level from stretching every cascade's depth range.

No magic extension constant, and it degrades gracefully: an empty scene fits to the box's own front
face. Ortho depth is linear, so a fit that comes out generous costs precision in proportion rather
than falling off the cliff a perspective near plane would.

The payoff is that the extracted frustum is now *already* the cast volume — `Frustum` needed no new
API, and cascades and local views cull through the identical `Cull(view.ViewFrustum, ...)` call.

### 4c. The distance test is gone

`BuildCascadeVisibility` and its `MAX_SHADOW_DISTANCE` radius test are deleted, not kept alongside
the frustum test. The last cascade's far plane already derives from that same constant, so the box
*is* the shadow distance expressed as a volume; the radius test on top of it measured from transform
position rather than bounds, and cut shadows off *inside* the box it was meant to approximate.
`MAX_SHADOW_DISTANCE` now has exactly one reader.

### 4d. Cost moved, not added

Culling prep grew: two pool scans per cascade (one to fit, one to cull) instead of one distance
scan, each transforming eight corners per caster. Against removing every out-of-cascade draw from a
10-metre cascade that was previously drawing everything within ~38.7 m, that is not close. Both
scans carry `PINE_PF_SCOPE` so it stays visible if it ever stops being true.

The obvious optimisation if it does: every cascade's view matrix shares one rotation basis and
differs only by a translation along the light axis, so the per-caster light-space AABB could be
computed once per frame and offset per cascade. Not built — it couples the fit to how the view
matrices are constructed, and there is no measurement asking for it yet.

**Verify:** shadows do not pop when the camera turns or objects move behind the light; "Cascade
views / casters drawn" in `DebugPanel` is far below the scene object count for a small scene, and
grows as the camera looks at more of it; frame time improves in a scene with geometry outside the
near cascade.

---

## Step 5 — Caching, invalidation, and the selection policy — DONE

Still spot-only. This is where the design either holds up or doesn't, and it is far easier to debug
with one view per light than with six.

### 5a. Invalidation — the dirty flags turned out to be unusable

The plan was to invalidate off `Entity::IsDirty()` / `Transform::IsDirty()`, working around the fact
that `SceneProcessor::Prepare` clears them at its own end. Reading how they are actually maintained
killed that outright:

- **`Transform::IsDirty()` means something different in the editor and in the game.** `RenderManager`
  calls `OnRender` on *every* transform before `Pipeline3D::Prepare` when `m_ProductionMode` is
  false, so in the editor the flag is always already cleared by the time anything looks at it. In
  production nothing does that, and the flag is instead cleared by `Transform::OnRender` inside
  `RenderBatch` — which only runs for objects that were *drawn*. A culled object would keep it
  raised forever, so every view containing one would re-render every frame.
- **Nothing clears a Light's transform flag at all.** No pass calls `OnRender` on a light. It would
  read as permanently moved and nothing would ever cache.

So invalidation is built on state it owns instead:

- **Casters:** `ModelRendererHintData` keeps `PreviousBounds` alongside `Bounds`, and
  `SceneProcessor` collects the renderers whose bounds changed into `SceneProcessorContext::MovedCasters`.
  Exact float comparison — the bounds are recomputed from the same inputs by the same code every
  frame, so an object that did not move reproduces them bit-for-bit, and an epsilon would only buy
  the ability to miss small movements. A mover is tested against each view with **both** boxes: it
  invalidates the view it left as much as the one it entered, and the view it left is the one that
  would otherwise keep a shadow of something no longer there.
- **The light:** the tile records the `ViewProjection` it was drawn with. Moving, turning, or editing
  range or cone angle all land there and nowhere else, so one matrix compare covers all of them.
- **Add/remove:** `CasterSetChanged`, from the renderer count. A count that did not change is not
  proof the set did not — one object destroyed and another created in the same frame reads as no
  change — but surviving that needs per-object identity tracking every frame to avoid one stale
  frame, which is the wrong trade.

The test stays O(movers), not O(scene): a cached view that has to walk every object to learn it can
skip its render has not saved much.

**The dirty-flag clear moved anyway.** It is now `SceneProcessor::EndFrame()`, called from
`Pipeline3D::Prepare` after the shadow work, which retires the `TODO` that admitted it was in the
wrong place. Nothing in the shadow path reads those flags any more, but what made the old placement
survivable — `Prepare` being the last scene-level work in the frame — stopped being true when the
local shadow pass moved in after it, and the next consumer would have inherited the bug silently.

### 5b. Where the cache lives

`TileState`, indexed by **atlas slot**, inside `Shadows.cpp`. Not on `ShadowAtlas::Slot`: the
allocator hands out rectangles and sweeps the ones nobody re-claimed, and what is drawn in them is
not its business. Not on the light either — the cached thing is the tile's pixels, and a light that
lost its tile has nothing left to remember.

It is resynced against the allocator's sweep at the top of every frame. That is not tidiness: the
component pool reuses slots, so a new `Light` can be constructed at a dead one's address and would
otherwise silently inherit its cached tile.

`ShadowView` gained a `NeedsRender` flag. The flag is on the view; the policy that sets it is not —
a reflection probe would answer the same question from entirely different inputs.

### 5c. Selection policy

Importance is the light's angular size — `range / distance`, which passes 1 once the camera is
inside the light's volume, and is zero when the light's whole sphere of influence is off screen.

**Scored against every active context, not the primary one.** The plan said primary; that is wrong
here for the same reason it was wrong for light culling in 1f — in the editor the primary context is
the *game* one, so a light important to the viewport you are actually looking at would lose its tile
to a light nobody can see. A light important to any live viewer is important.

Hysteresis is expressed as a handicap inside a single sort rather than as a separate pass:

| | selection score |
|---|---|
| challenger | `importance` |
| incumbent | `importance * 1.25` |
| incumbent held < 0.5 s | `FLT_MAX` — cannot be evicted yet |

The budget is spent on **winners taken, not list position**. An incumbent that goes off screen while
still inside its residency window sorts to the very top while winning nothing, and counting it as a
rank would let it deny a tile to a light that is genuinely on screen.

Losers that hold a tile keep it while `ShadowFade` ramps down, and are not counted against the
budget — they are leaving. That is what `Params.z` has been carrying since step 1.

### 5d. Budget

`GraphicsSettings::LocalShadowTileBudget`, in tiles: 2 / 4 / 8 / 8 across Low→Ultra. Tiles rather
than per-type counts, so a scene with five spots and no point lights just works and a point light
simply costs six.

It bounds worst-case cost rather than memory. Holding a valid tile is nearly free — that is what the
cache buys — so the number that hurts is how many tiles can be re-rendered in one frame. The debug
"Force tile limit" slider stays the way to exercise the exhaustion path on demand.

### 5e. Also fixed

Turning shadows off at runtime left every light pointing at the view it held, sampling a tile nothing
refreshed any more — the shadows froze in place instead of disappearing. `ClearLocalViews` handles
the off branch.

**Verify:** walk a room with 4+ shadowed spots — no flicker, no popping, and the debug panel shows
`re-rendered 0, cached N` whenever nothing is moving. Drop the tile budget below the number of spots
and confirm the losers fade rather than pop, and that the per-tile `held` time keeps climbing instead
of resetting every frame (a reset every frame is thrash).

---

## Step 6 — Point lights — DONE

Done in two halves: generalising step 5's machinery from "one tile per light" to "N tiles per
light", then the cube itself. The split was worth it — every point-light-specific bug that showed up
was in the second half, where it belonged.

### 6a. Groups, because "four faces of six" is not a partial success

`ShadowAtlas::Acquire` took a size and an owner and returned one slot. It now takes a count and
fills an array, **all or nothing**: a point light holding four of its six faces is not two-thirds
shadowed, it has a hard discontinuity along every edge between a face it got and one it didn't.
Nothing is written to a slot until the whole group is known to be available, so the failure path has
nothing to undo. Slots come back in ascending index order, which is what makes face → tile stable
across frames — and tile stability is the entire precondition for caching.

Two things in step 5 turned out to be shaped around every light costing exactly one tile:

- **Incumbency was read off a tile.** `FindHeldSlot` returned the first slot an owner held, and
  residency and the challenger margin were read from it. Residency, fade and importance are
  properties of the *claim*, not of any one tile — a point light that has held its faces for two
  seconds has held them for two seconds, not two seconds each — so they moved to a per-light
  `LightShadowState`. Cache validity stayed per tile, because that genuinely is per tile: each face
  is a different picture.
- **The budget counted winners.** It was only "a tile budget" because every winner cost one. It
  spends tiles now, which is what makes the claim in 5d true rather than accidentally true.

### 6b. Tile sizes finally do something — corrected after first run

The first version tied size to light type: spots `Large`, point faces `Medium`. It looked visibly
rough at 256², and the justification was wrong anyway — the claim was that a cube face covers 90°
where a spot cone covers a narrow slice, but a spot at the **default 45° outer angle covers about
94°**. Same angle, half the linear resolution, for no reason.

Two changes:

- **Size follows importance, not light type.** `importance >= 1.0` (the camera is inside the light's
  volume) takes `Large`, below that `Medium`, with a demote threshold of 0.7. The gap is hysteresis
  and it is load-bearing: changing size means changing tile, which means the new tile is cold, which
  means a full re-render — a light hovering on a single threshold would re-render every frame, which
  is the cost caching exists to remove, reintroduced by the thing meant to improve quality. That is
  what a multi-size atlas is actually for, and the importance score the selection policy already
  computes is exactly the input it wanted.
- **`TileSize::Small` is gone, and the atlas re-split 3 Large + 1 Medium.** Small was a whole
  quadrant — a quarter of the atlas — holding 64 tiles at 1/16 of the atlas edge. `SHADOW_VIEW_COUNT`
  caps the engine at 32 live views, so **64 tiles of anything was unreachable by construction**. The
  space went to Large: 12 Large + 16 Medium = 28 tiles against a 32-view ceiling, which is a
  well-matched atlas instead of an arbitrary one. At the default 2048 that is 512² and 256².

The tile budget moved with it, to 6 / 12 / 16 across Medium→Ultra. A point light costs six tiles, so
any budget under 6 silently means "point lights never cast" — the old 4 at the Medium preset was
exactly that.

### 6c. Seams — three fixes, and one of them was a bug already shipped

The plan budgeted an afternoon of squinting at a sphere in a room. Most of that was avoided by
getting the third fix right up front rather than discovering it visually.

1. **Widened faces.** Each face is rendered slightly over 90°, by exactly enough to carry a two-texel
   border. Derived from the tile resolution rather than a fixed angle — the border has to be a
   constant number of *texels*, and half a degree is generous on a 128px tile and not enough on a
   1024px one.
2. **The half-texel clamp was wrong, and had been since step 3.** It inset by half a texel of the
   *atlas* where `projected.xy` spans one *tile* — too small by exactly the tile-to-atlas ratio, a
   factor of four for a Large tile. Spots hid it because their +4° cone widening supplied a border
   anyway. It is one of the two fixes point lights actually depend on, so it would have looked like
   a point-light bug.
3. **The normal offset is applied before the face pick, not after.** This is the one that would have
   cost the afternoon. Picking a face from the un-offset position and then offsetting can carry the
   sample past the edge of the face that was chosen; it then projects outside `[0,1]` and is treated
   as unshadowed — a *bright* seam along every cube edge, which reads as a lighting bug rather than
   a mapping one. The face border is about one texel of angle while the offset is 0.02 world units,
   so within about five metres of the light the offset wins comfortably. All six faces carry
   identical params, so reading them off the first view before the pick is exact.

The shader branches on `shadowViewCount > 1`, not on a light type. By the time anything reaches
`SampleLocalShadow` there are only views, and how many of them there are is the whole difference —
same shape as the CPU side.

### 6d. `Cull` gained `restrictTo`, plus a sphere overload

As planned, except the plan's caster set was to come from `ModelRenderer::CastShadows`, which **does
not exist** — `CastShadows` is on `Light`. The superset is the light's sphere of influence instead,
which needed `Cull(center, radius, visibility)`. That is not a special case of the frustum test: a
sphere is the natural bound for anything that *radiates* rather than projects, and it is a fraction
of the cost of six planes.

So a point light culls once against its sphere and restricts its six face culls to what that found —
six full-scene frustum passes become one cheap pass plus six that skip almost everything. Objects
excluded by `restrictTo` are not counted as culled; they were never candidates, and counting them
would make the two statistics mean different things depending on whether a restriction was passed.

**Culling moved from `RenderLocalViews` to `PrepareLocalViews`** to make this possible. Prepare is
where a light is still visible *as a group*; by the time `RenderLocalViews` walks `m_LocalViews`
there are only views, which is the right shape for rendering and the wrong one for this.

**Verify:** a point light in a room casts in all directions; no seams at cube edges, bright or dark;
the panel shows six Medium tiles under one light name; walking past several point lights doesn't
thrash, and the `held` time keeps climbing.

---

## Step 7 — Fold cascades into the atlas — DONE

The second storage path is gone: no `Texture2DArray`, no `Shadows` UBO, no sampler 16, no
`hasDirectionalShadowMap`, no `AddDirectionalShadowMap`, no `ShadowMapResolution`. One atlas, one
sampler, one allocator, one render loop.

### 7a. Pinned reservations

`ShadowAtlas::Reserve` takes tiles out of circulation permanently. A pinned tile is exempt from the
sweep, which is the exact opposite of how everything else in the allocator works — and that is the
point. Mark-and-sweep is right for a contended resource where not asking means you stopped caring,
and wrong for one whose owner is structural. Cascades exist whenever the level has a sun; a frame in
which they lost a contest for space would just be a frame with no sun shadows.

The cascades' owner is a **token, not a `Light`**: the allocator only compares owner pointers, and
there is nothing stable to point at — a level can swap its sun or have none without the reservation
changing hands. That immediately caught a latent crash in the debug panel, which cast `Slot::Owner`
to `Light*` and read a name off it. `TileDebugInfo::Reserved` now answers that question in the one
place that knows.

### 7b. The atlas re-split, and named honestly

`TileSize` is `Half` / `Quarter` / `Eighth` — a fraction of the atlas edge, which is what a size
class is. `Large`/`Medium` described nothing, and the old third value was named `Small` while being
1/16.

Layout: two `Half` quadrants (pinned by the cascades), one `Quarter` quadrant of four, one `Eighth`
quadrant of sixteen. **Half the atlas goes to two tiles**, and it is spent whether or not the level
has a sun. That is the honest cost of pinning, and it is still far cheaper than what it replaced.

The default atlas resolution moved 2048 → 4096, and there is now exactly one shadow quality knob
where there were two that could disagree.

| | before | after |
|---|---|---|
| cascade array | 4096² × 2 layers, D24/32 — **134 MB** | — |
| local atlas | 2048², D16 — 8 MB | — |
| shadow atlas | — | 4096², D16 — **34 MB** |
| per cascade | 4096² | 2048² |
| nearby local light | 512² | 1024² |
| distant local light | 256² | 512² |

**Local lights doubled, cascades halved, total memory down about 4x.** The cascade drop is the one
visible regression in the trade; `ShadowAtlasResolution` 8192 puts them back at 4096² and still costs
less than the old array alone did.

D16 holds up for cascades, which is worth writing down because it looks wrong: ortho depth is
**linear**, so 16 bits over even a 150 metre cascade is millimetre resolution — far finer than the
separation front-face culling already provides. If banding ever appears there, `Depth32F` is the
one-line answer at double the memory.

### 7c. One render path

`RenderViews` draws a run of views into the atlas and has no idea which kind it is looking at.
Everything that used to differ is carried by the view: its tile, its bias pair, and — new here —
`ShadowView::FaceCulling`, because a cascade culls front faces (the light is effectively infinitely
far away, so recording back faces hides acne for free) and a local light culls back faces (at short
range front-face culling peter-pans and fails outright on single-sided geometry). Those two genuinely
disagree and now share a pass, so the choice had to become data.

That deletion is what this step actually bought. The memory was a side effect.

### 7d. The lookup

`SampleShadowView(viewIndex, samplePosition, pcfTaps)` is the whole shadow lookup, for everything.
Local lights pass 0 taps — one hardware 2x2 fetch, because a fragment may sample seven of them.
Cascades pass 1, giving the 3x3 grid the directional path always had; dropping to a single tap would
have been a visible softness regression on a feature that worked.

Cascade selection by camera distance sits in `ComputeShadowFactor`, cube-face selection in
`SampleLocalShadow`, and neither knows about the other. `shadowViews[0 .. CASCADE_COUNT-1]` is
reserved for the cascades, mirroring the pinned tiles — two fixed reservations that must agree, and a
light landing on a cascade's view would inherit its matrix with no symptom until someone looked.

`FrameReset` now clears `ShadowViewIndex` on every light, not just the colour. The shader decides
"is there a directional shadow" from that field alone now, and `lights[0]` is the directional slot
whether or not the level has a sun — a stale index from a level that did would sample a cascade
nothing wrote.

**Verify:** directional shadows still work and look the same modulo resolution; no banding or acne in
either cascade; the atlas viewer shows two large tiles re-rendering every frame plus the local tiles;
turning the sun off and on leaves nothing stale; a level with no directional light has no shadow
where there should be none.

---

## What this leaves standing

Named now rather than discovered later.

- **The object light slot layout is now 5 point + 2 spot** (raised from 1 spot after step 5, so a
  hand-held light and a world light can reach the same surface — at one slot, a flashlight occupies
  the only slot every object it touches has). It was asserted in four unconnected places
  (`Specifications.hpp`, `SceneLightsProcessing.cpp`, `Renderer3D::AddLight`, the unrolled
  subscripts in `generic.fragment.glsl`), and `AddLight` was the one that had been shaped around
  having exactly one spot slot — it wrote `SPOT_LIGHT_OFFSET` directly where the point branch five
  lines above it scanned for the first free slot. It scans now. **`COUNT` is capped at 7 by
  `lightDir[8]` in the generic shader's varying block; going past that grows the block and costs
  three interpolated floats per fragment on every material, so 6→7 was free in a way 7→8 is not.**
- **Forward-shading light count is unchanged.** Five point lights per object stays five, shadowed or
  not. Clustered forward is the door this leaves open, and putting the shadow index in the *light*
  entry rather than the per-object instance slots is the deliberate reason a `ShadowView` array
  survives that transition.
- **Terrain: out of scope, deliberately.** `TerrainRendererComponent` renders through its own path,
  already has no light hint data, and both casts and receives shadows. It is the second caller that
  will want `CastShadows`/`ReceiveShadows` to live somewhere shared rather than on `ModelRenderer`
  specifically — knowing that in advance is the difference between moving the flags later and
  duplicating them later.
- **Editor gizmos and terrain read `Instances[0]`'s arbitrary global light subset**, and now its
  arbitrary shadow views with it. Shadows make that existing wrongness more visible, not worse.
- **`ReceiveShadows` on `ModelRenderer`** needs to reach the fragment shader per object.
  `Instance` carries `ivec4 lightIndices[2]` = 8 ints, and raising the spot count to 2 took that from
  6 used to 7 — so there is exactly one int of padding left for a per-object flags word with no
  std140 change, and it is now the last one. Anything that wants two per-object ints will have to
  grow `Instance`, which is `MAX_INSTANCE_COUNT` times more expensive than it sounds. The branch is per-object and perfectly coherent across
  a draw — effectively free. Fold into step 3 or 6, wherever it first matters.
- **The SSBO backend is not built and should not be.** Step 1e removes the only good argument for
  it. Clustered forward light lists is the plausible first feature that genuinely *needs* one, and
  that is a much better place to build it, because it cannot work without it.

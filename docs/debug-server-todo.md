# Debug server TODO

For available routes and scene-authoring examples, see the [Editor API guide](debug-server.md).

Track the work needed to author and inspect 3D scenes through Pine's editor.
The remaining work is ranked by usefulness for scene authoring and investigation,
after the completed capabilities below. This is a recommendation, not a commitment
to implement every item; shared prerequisites can be implemented together.
Check an item off after its complete workflow has been verified, and link its
protocol documentation when it adds a public operation.

## Constraints

- Keep protocol parsing, schemas, adapters and agent-specific helpers under
  `Editor/src/DebugServer/`.
- Use Pine's public operations for component behavior. When those operations are
  insufficient, discuss a proper subsystem change before expanding support.
- Keep 2D components and 2D camera controls out of scope.
- Keep scene writes explicit and validated. The binary-to-JSON dump remains an
  inspection format; it does not become an unrestricted live editing interface.
- Preserve the existing localhost enablement and main-thread execution model.

## Available now

- [x] Inspect status, logs, entities, serialized component state, assets and rendering statistics.
- [x] Capture Level and Game viewport PNGs and load a level through the debug server.
- [x] Create entities in batches, including parenting to earlier request references or existing entities.
- [x] Configure and update Transform, ModelRenderer, Light and perspective Camera through dedicated adapters.
- [x] Discover writable properties and defaults through `/edit/schema`.
- [x] Validate an entire batch before mutation; report completed operations on execution failure.
- [x] Read/restore the editor camera, look at a point, and frame entities or hierarchies.
- [x] Synchronize camera changes with mouse navigation and clear residual movement.
- [x] Import a local source file through the editor importer, copying it into the project and loading the result. Supports synchronous re-import and per-entry failures; see [asset import](debug-server-import.md).

Current contracts: [scene editing](debug-server-editing.md),
[editor camera](debug-server-camera.md),
[rendered observations](debug-server-observation.md),
[history and persistence](debug-server-history.md),
[request retries and completion](debug-server-requests.md). The original transport/dump design is in
[the debug-server plan](plans/debug-server.md).

## Reliable visual feedback — complete

Implemented and verified through [rendered observations](debug-server-observation.md).
Edits and camera operations return tokens for a subsequent rendered capture.

- [x] Add a way to capture a viewport **after a specified edit or camera operation
  has actually been rendered**. Waiting must not block the main thread that needs
  to render the frame.
- [x] Return frame identity and camera/viewport metadata associated with that
  capture. Image and metadata must describe the same rendered view.
- [x] Define behavior for a hidden viewport, a level replacement, and changes made
  through the UI while an observation is pending. Return an explicit failure when
  the requested capture cannot be produced.
- [x] Provide one convenient observation workflow combining the capture with
  relevant entity state and recent errors, with clear timing for each part.
- [x] Make log retrieval incremental so one edit can be checked without repeatedly
  reading the whole log history.

Acceptance: create a model and light, frame them, capture the resulting view, move
the model, and capture again. Both captures must demonstrably include their
requested changes without arbitrary client sleeps.

## Reliable retries and reusable verification — complete

Implemented and verified through [request retries and completion](debug-server-requests.md).
Identified mutations retain their original result; queued cancellation and deadlines
prevent later execution. The reusable HTTP recipes cover editing, retries and captures.

- [x] Give mutating requests an identity so retrying a creation after a lost reply
  does not create duplicates. Reject reuse of an identity with a different payload.
- [x] Expose request completion/status with bounded retention. Distinguish a
  rejected request from a pending request and an operation that may have executed.
- [x] Define timeout/cancellation behavior for queued and already-running work.
  Expired queued work must not execute later.
- [x] Identify the current scene generation so clients can detect references made
  stale by loading a level or stopping play mode. See [observation tokens](debug-server-observation.md).
- [x] Preserve a repeatable integration verification recipe using a temporary
  project: validation leaves state unchanged, repeated patches compose correctly,
  references resolve, camera state persists, and framed bounds fit the viewport.
  Reuse the current build/manual-verification approach; no new test framework is
  required for this work.
- [x] Add interrupted-reply, retry, scene-replacement and capture-ordering cases as
  the corresponding behavior becomes available.

## Everyday scene editing — complete

- [x] Add supported components to existing entities and remove them by component
  ID. Preserve the required Transform and enforce component dependencies. See
  [component addition/removal](debug-server-editing.md#add-and-remove-components).
- [x] Rename entities and edit active/static flags through the appropriate setters.
  See [entity property updates](debug-server-editing.md#update-entity-properties).
- [x] Reparent existing entities, including detaching to the scene root. Reject
  cycles and define local-transform versus world-transform preservation using
  Pine's actual hierarchy semantics. See [reparenting](debug-server-editing.md#reparent-entities).
- [x] Delete an entity or hierarchy while keeping selection and outstanding
  references valid or explicitly invalidated. See [entity deletion](debug-server-editing.md#delete-entities).
- [x] Duplicate entities/hierarchies with a documented identity and reference policy.
  Supports Transform, ModelRenderer, Light, perspective Camera, primitive Collider and RigidBody; other components reject the batch.
  See [entity duplication](debug-server-editing.md#duplicate-entities).
- [x] Extend schema discovery to describe operation envelopes and reference rules,
  alongside the existing component property descriptions. See
  [operation and reference discovery](debug-server-editing.md#operation-and-reference-discovery).
- [x] Provide writable-state readback in the same representation accepted by edits,
  so enums and asset references do not need to be inferred from the binary dump.
  See [writable-state readback](debug-server-editing.md#writable-state-readback).

## Undo and persistence — complete

Implemented and verified through [history and persistence](debug-server-history.md).

- [x] Integrate debug edits with editor history: one successful batch should be one
  undo step, with redo restoring the same intended state and identities.
- [x] Define history behavior after a partially executed batch. Undo and automatic
  rollback are separate guarantees; keep partial-failure reporting explicit.
- [x] Verify that history restoration performs necessary component side effects.
  Existing binary snapshots alone do not guarantee setter behavior is reproduced.
- [x] Add explicit level save and save-as operations with clear destination and
  overwrite behavior, plus inspection of unsaved changes.
- [x] Verify save → reload preserves the authored hierarchy, component properties
  and asset references. Saving should not be an implicit side effect of editing.

Acceptance: build a small scene, modify a group, undo and redo it, save it, reload
it, then frame and capture it with the expected state intact.

## Additional 3D capabilities — complete

- [x] Add a scene Camera adapter for projection and clipping properties. See
  [scene cameras](debug-server-scene-camera.md).
- [x] Add an explicit level operation to choose the active game camera; verify the
  result through the Game viewport. Includes history and save/reload verification;
  see [game camera selection](debug-server-scene-camera.md#select-the-game-camera).
- [x] Add primitive Collider support with creation-on-play behavior and explicit
  interaction with RigidBody. See [3D physics authoring](debug-server-physics.md).
- [x] Add RigidBody support with validated mass, type, gravity and lock settings.
  Verify actual physics objects, including fresh creation after Stop/edit/Play.

## Pick surfaces from captures — complete for model meshes

Implemented through [surface picking](debug-server-picking.md). Request an
observation with `picking: true`, then query its retained capture and PNG pixel.
This first workflow covers solid ModelRenderer surfaces while stopped; terrain
and material-accurate visibility remain follow-up work.

- [x] Pick from a capture and pixel coordinate, returning the entity, mesh,
  world position and surface normal where available.
- [x] Define the pixel coordinate convention and tie the result to the referenced
  capture's camera and scene state. Later camera or geometry changes preserve the
  captured answer. Expired, evicted and scene-replaced captures explicitly fail.

Verified: pick a rotated wall through a parent transform, move the camera and
wall, and query the original surface unchanged. Also verifies misses, normals,
resized images, mesh selection, occlusion, bounded retention and scene reload.

## Recommended next work, in priority order

Add each adapter as a complete workflow: advertised properties, validation,
application, readback and observable behavior. Declare units, enum names, asset
types and related-field constraints in the same place as the adapter.

### 1. Spatial queries and placement

Makes placement reliable without estimating geometry from screenshots or treating
an asset's pivot as its contact point. World-space inspection is also a useful
foundation for picking and the filtered queries below.

- [x] Expose fresh world bounds, dimensions, world/local transforms and orientation
  vectors for selected entities, including terrain where applicable. Reuse framing
  calculations where appropriate and declare which geometry the bounds cover. See
  [spatial measurements](debug-server-spatial.md) for the read-only batched query.
- [x] Add raycasts and overlap queries usable in stopped edit mode, including
  visible geometry without physics colliders. Distinguish bounds overlap from
  actual geometry intersection in the query contract. Model/terrain surface
  raycasts and bounds-only overlaps are verified; see
  [spatial intersections](debug-server-spatial.md#raycasts-and-bounds-overlaps).
- [ ] Add placement helpers: rest on a surface, mount flush against a wall, align
  bounds, and offset relative to another entity. Define anchors, clearance,
  coordinate space and units explicitly; account for parent transforms.
- [ ] Aim an entity, such as a spotlight, at a world point with an explicit forward
  axis and up direction.

Acceptance: place a crate on uneven ground and a lamp against a rotated wall, then
aim the lamp at a picked point. Read back the resulting transforms and inspect the
contact and clearance; each placement must participate in undo.

### 2. Lighting and shadow diagnostics

Most useful for explaining a rendering problem. Ravenholm's light conversion
highlighted the need to distinguish cone boundaries, light-slot selection and
shadow allocation instead of inferring the cause from the final image.

- [ ] Report the lights actually assigned to a selected model's lighting slots,
  with entity IDs and names. Extend the existing terrain light-slot inspection
  rather than introducing a different representation for the same concept.
- [ ] Expose shadow statistics and per-light allocation: assigned views, tile
  resolution, importance, cache reuse, rendered views, and reasons for denied or
  downgraded shadows where the renderer can provide them. Start with the existing
  `Shadows::GetStatistics()` and `GetTileDebugInfo()` facilities.
- [ ] Capture the shadow atlas with tile-to-light identification and offer
  optional light cone/range overlays.
- [ ] Associate diagnostics with a rendered frame and context so comparisons
  against captures describe the same state.

Acceptance: inspect a problematic lit surface, identify its assigned lights and
their shadow allocations, and compare point/spot configurations from the same view
with captures and shadow statistics.

### 3. Filtered and batched scene inspection

Reduces request overhead and makes level probing practical. The Ravenholm lamp
change required a hierarchy read, twelve individual light reads, and parent reads
just to establish the reference settings and placement.

- [ ] Query entities by component type, name and hierarchy, with optional component
  properties and world transforms in the response.
- [ ] Read a specified set of entities in one request without requiring a capture.
- [ ] Query entities within a radius or bounds, explicitly defining whether the
  test uses pivots or geometry bounds.
- [ ] Bound response sizes and define truncation or pagination; sample each batch
  coherently and identify its scene generation.

Acceptance: fetch all lights and their world transforms in one request, then find
nearby scene objects around a selected lamp without downloading the whole level.

### 4. Independent inspection captures

Allows an agent to inspect the scene while the user continues navigating the editor.

- [ ] Render from a supplied camera pose and image size without moving the user's
  editor camera or requiring a particular viewport tab to be visible. Preserve
  the existing capture ordering and frame metadata guarantees.
- [ ] Offer optional selection outlines or entity labels alongside clean captures.
- [ ] Add depth, normals, bounds and collider visualization on demand, using
  existing rendering facilities where available. Keep diagnostic modes scoped to
  the requested capture.

Acceptance: capture an object from several supplied poses while the user navigates
another view; verify the editor camera is unchanged and each image has matching
camera and frame metadata.

### 5. Atmosphere, materials and reusable props

Completes more of the visual authoring workflow after objects have been placed.

- [ ] Expose ambient light, fog and skybox through typed read/write operations.
- [ ] Add material parameter authoring and per-mesh material assignments. Make
  shared-material effects explicit and provide the relevant asset persistence
  workflow; saving a Level alone does not save other modified assets.
- [ ] Spawn an existing Blueprint asset, returning the created hierarchy's IDs.

Acceptance: adjust a scene's atmosphere, edit and assign a prop material, spawn a
Blueprint, and verify the intended scene and asset changes survive save/reload.

### 6. Play controls and controlled simulation

Enables verification of placed objects and gameplay interactions after authoring.

- [ ] Add explicit play/pause/stop controls. Keep ordinary scene writes restricted
  to stopped mode until runtime mutation semantics are deliberately supported.
- [ ] Expose runtime transforms, velocities and contact information for selected
  objects, with the simulation step or frame that produced the readings.
- [ ] Advance a specified number of simulation steps once physics and script
  updates can be advanced coherently. Define the timestep and participating
  systems rather than treating an arbitrary rendered frame as a simulation step.

Acceptance: start a scene, pause and step a falling object until it settles, inspect
its transform and contacts, then stop and verify restoration of the authored scene.
Use the same controls to check a traversable doorway when a suitable player exists.

## Later, driven by actual scene work

- [ ] Extend capture picking to terrain and material-accurate visibility (alpha
  cutouts, transparency and shader deformation). The current `model-surfaces`
  contract deliberately reports solid model geometry and excludes other geometry
  from occlusion; see [coverage](debug-server-picking.md#geometry-coverage).

- [ ] Reuse existing project textures/materials when importing agent-authored GLBs.
  For the initial Blender sub-agent trial, duplicate imported materials/textures
  are explicitly accepted. Later, investigate matching embedded resources to
  existing assets and preserving references across re-imports; do not block
  asset generation on deduplication.
- [ ] Add camera orbit/dolly conveniences if repeated authoring work needs them.
- [ ] AudioSource/AudioListener authoring and a way to verify playback behavior.
- [ ] CharacterController support with explicit movement and physics semantics.
- [ ] Script component authoring after assembly resolution, field types, references
  and lifecycle behavior have a defined editing contract.
- [ ] Expand terrain authoring beyond the existing inspection, sculpting and
  painting routes when concrete scene work requires it, including terrain asset
  creation and TerrainRenderer assignment.
- [ ] Revisit automatic batch rollback once supported operations have a reliable
  restoration path; do not advertise atomic execution before then.

Orthographic inspection of 3D scenes can be reconsidered separately if needed.
It is not part of the current perspective-camera scope and does not imply adding
2D gameplay support.

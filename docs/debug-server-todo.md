# Debug server TODO

For available routes and scene-authoring examples, see the [Editor API guide](debug-server.md).

Track the work needed to author and inspect 3D scenes through Pine's editor.
The order below is a recommendation, not a commitment to implement every item.
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

## 1. Reliable visual feedback — complete

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

## 2. Reliable retries and reusable verification — complete

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

## 3. Everyday scene editing — complete

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

## 4. Undo and persistence — complete

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

## 5. Expand useful 3D capabilities

- [x] Add a scene Camera adapter for projection and clipping properties. See
  [scene cameras](debug-server-scene-camera.md).
- [x] Add an explicit level operation to choose the active game camera; verify the
  result through the Game viewport. Includes history and save/reload verification;
  see [game camera selection](debug-server-scene-camera.md#select-the-game-camera).
- [ ] Expose relevant level settings such as ambient light, fog and skybox through
  typed operations.
- [ ] Spawn an existing Blueprint asset, returning the created hierarchy's IDs.
- [x] Add primitive Collider support with creation-on-play behavior and explicit
  interaction with RigidBody. See [3D physics authoring](debug-server-physics.md).
- [x] Add RigidBody support with validated mass, type, gravity and lock settings.
  Verify actual physics objects, including fresh creation after Stop/edit/Play.
- [ ] Add explicit play/pause/stop controls for checking the resulting scene.
  Keep ordinary scene writes restricted to stopped mode until runtime mutation
  semantics are deliberately supported.

Add each adapter as a complete workflow: advertised properties, validation,
application, readback and observable behavior. Declare units, enum names, asset
types and related-field constraints in the same place as the adapter.

## 6. Spatial tools and richer observation

- [ ] Expose fresh world bounds, dimensions, world/local transforms and orientation
  vectors for selected entities. Reuse framing calculations where appropriate.
- [ ] Pick an entity and world position from a viewport coordinate, returning the
  surface normal where available. Tie coordinates to the referenced capture.
- [ ] Add placement helpers: place on a surface, align bounds, and offset relative
  to another entity. Define the anchor, coordinate space and units explicitly.
- [ ] Offer optional selection outlines or entity labels on captures, alongside
  clean images for judging the scene's appearance.
- [ ] Add useful rendering diagnostics on demand, such as depth, normals, bounds
  or collider visualization, using existing rendering facilities where available.
- [ ] Add camera orbit/dolly conveniences if repeated authoring work needs them.

## Later, driven by actual scene work

- [ ] Reuse existing project textures/materials when importing agent-authored GLBs.
  For the initial Blender sub-agent trial, duplicate imported materials/textures
  are explicitly accepted. Later, investigate matching embedded resources to
  existing assets and preserving references across re-imports; do not block
  asset generation on deduplication.
- [ ] Material authoring and assigning materials to individual model meshes.
- [ ] AudioSource/AudioListener authoring and a way to verify playback behavior.
- [ ] CharacterController support with explicit movement and physics semantics.
- [ ] Script component authoring after assembly resolution, field types, references
  and lifecycle behavior have a defined editing contract.
- [ ] Single-frame simulation stepping once timing and physics/script updates can
  be advanced coherently.
- [ ] Terrain authoring and bounds support when a concrete task requires it.
- [ ] Revisit automatic batch rollback once supported operations have a reliable
  restoration path; do not advertise atomic execution before then.

Orthographic inspection of 3D scenes can be reconsidered separately if needed.
It is not part of the current perspective-camera scope and does not imply adding
2D gameplay support.

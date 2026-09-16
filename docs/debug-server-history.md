# Debug-server history and level persistence

Successful `POST /edit` batches now register one command in the editor's existing
128-entry undo history. Ctrl+Z/Ctrl+Y and the HTTP endpoints use the same stack.
A new edit after undo discards the redo branch. A held UI component edit is
finished before a debug batch samples its starting state.

## Operations

| Method and route | Body | Result |
|---|---|---|
| `GET /history` | — | `undoCount`, `redoCount`, `limit`, `sceneGeneration` |
| `POST /history/undo` | `{}` or empty | History counts, `applied`, observation token |
| `POST /history/redo` | `{}` or empty | History counts, `applied`, observation token |
| `GET /level/status` | — | `path`, `id`, `hasDestination`, `unsavedChanges` |
| `POST /level/save` | `{}` or empty | Save the active project Level to its current destination |
| `POST /level/save-as` | `{"path":"levels/example","overwrite":false}` | Save to a project-relative virtual path and make that Level active |

Unknown body fields are rejected. Undo, redo and saving require stopped play mode
(409 otherwise). An empty history returns 200 with `applied: false`. Reads remain
available during play; `/level/status` returns `unsavedChanges: null` with a reason
until stopped, since runtime state is not the authored scene.

Mutations use the existing [retry identity and completion rules](debug-server-requests.md).
Retrying an identified undo, redo or save returns the original response, even if
later changes have occurred. It does not execute another step. Mutation responses
include an [observation token](debug-server-observation.md) for a subsequent capture.
`/edit/schema.history` advertises the history routes and batch/failure policies.

## What a batch restores

History records the before/after values of affected entities and components. It
restores names, entity active/static flags and tags, local transforms, model and
material references, mesh indices, lights, component active flags, and renderer
stencil settings. Perspective Camera properties and active game-camera selection
also participate in history; see [scene cameras](debug-server-scene-camera.md).
History also restores parent/child order, component order and scene listing order. Unchanged entities and component values are not rewritten.

Creation, duplication, component removal and entity deletion restore their original
**persistent entity and component IDs** when undone/redone. Pool slots, raw pointers
and managed objects can change; reacquire objects by their persistent IDs. Deleted
objects become unavailable while absent and can become available under the same IDs
when history restores them. Batch `ref` names and retained HTTP results remain
historical request results; they are not rewritten by undo.

Restoration uses the same component adapters/public setters as editing, plus public
entity lifecycle operations. It does not apply binary snapshots with `LoadData()`
to live components. Transform hierarchies and rendering caches are dirtied as
needed, and recreated components receive fresh runtime caches. Deleting objects
during undo/redo uses the normal debug deletion selection/drag cleanup. Undo does
not restore old editor selections or camera navigation.

Deletions are restricted to hierarchies containing Transform, ModelRenderer, Light,
Camera, primitive Collider and RigidBody. Unsupported components anywhere in the
proposed deleted hierarchy reject the **entire batch before mutation**. Supported property/entity operations can still
modify an entity that also has unsupported components. Component changes/removals
and deletions also validate the original values that undo will need to restore.
Invalid legacy/native values that cannot pass through the restoration adapter
reject the batch with a `beforeState` validation path; repair those values through
their owning subsystem before editing them through this API.

A successful batch, including a batch with no net state change, reports
`history: "recorded"` and occupies one step. Validation rejection leaves history
and the scene unchanged.

## Failures and intervening changes

Execution failures retain completed operations and any partial effects of the
failing operation. They report the existing operation/completion details and
`history: "cleared"`. Both undo and redo history are cleared; **no automatic
rollback occurs**. If recording history fails after all edits complete, the result
is 500 with the completed count, error, and `history: "cleared"`.

Before restoring a batch, history checks affected entity state, scene membership
and ordering, asset/property validity and pool capacity. Direct UI/native changes
outside recorded commands can make a snapshot inapplicable. A restoration failure
returns 500 with `error`, `historyCleared: true`, `stateMayHaveChanged: true` and an
observation token; both history stacks are cleared. Preflight failures leave scene
values unchanged, but a failure during application can leave partial restoration.
Inspect current state before continuing. The UI logs restoration errors too.

Loading/replacing a scene, including stopping play mode, invalidates history through
its scene generation. Saving and save-as keep live entity IDs and history intact.
Existing UI commands retain their own restoration behavior; this work adds debug
batch commands, not undo support to every otherwise-untracked editor interaction.

## Saving and unsaved changes

`/level/save-as.path` is a lowercase virtual asset path relative to the current
project's `assets/` directory, without `.passet`, e.g. `levels/courtyard`. The server
creates missing parent directories. Absolute paths, traversal, symbolic links,
noncanonical separators and conflicts with another asset type are rejected.
The final destination always stays inside the project's assets directory.

Save-as defaults to `overwrite: false`; an existing destination rejects the request.
With `overwrite: true`, an existing destination must be a loaded project Level
backed by a regular file. Its asset ID is preserved. A new destination receives a
new asset ID. Files that appeared outside the asset manager must be loaded before
they can be overwritten. Ordinary `/level/save` overwrites the active Level's
current project destination; an untitled Level returns 409 and requires save-as.
Neither endpoint implicitly saves other assets.

Saving captures the live scene through Pine's Level/Blueprint serialization,
compresses it to a temporary sibling file, verifies its contents, then installs the
file at the destination. The old destination is retained if writing/verification
fails. `overwrite: false` also refuses a destination created by another writer
after validation. Responses include the virtual `path`, Level asset `id`,
`fileWritten: true` and `unsavedChanges: false`. Execution failures return 500 with
`fileWritten` and `stateMayHaveChanged`; a file can have been saved even if a later
asset-registration step failed. Saving is not itself undoable, and undo never
rewrites files.

`/level/status` compares the complete serialized scene payload with the active
Level's file, excluding asset header timestamps and IDs. It detects changes from
debug edits, undo/redo and UI edits to serialized scene state. Untitled/missing
files report unsaved changes. This comparison covers serializer-defined authored
state, not every runtime member or other assets. A serialization format upgrade
can also make a loaded older file differ until it is saved again.

Level reload creates fresh scene IDs as before. Hierarchy, writable component
properties and asset references survive save/reload. Blueprint capture now also
preserves entity tags and component active flags; older files without a component
active field load it as true.

## Verification

Build the Editor and use the [disposable-project launch recipe](debug-server-observation.md#verification)
with port 19032 and the Level viewport selected. Then run:

```sh
python3 Editor/src/DebugServer/Verification/verify-history.py \
  --url http://127.0.0.1:19032 --output /tmp/pine-history-results
```

It exercises all eight editing operations through undo/redo, grouped changes,
persistent IDs, component/hierarchy order, validation preserving redo, retries,
save/save-as and overwrite rules, unsaved-state changes through undo/redo, and
save → reload → frame → capture. Inspect `saved.png` and `reloaded.png`. It writes
Level files only in the disposable project's `verification/` directory. Close the
disposable Editor afterwards.

Also verify with native fixtures: unsupported deletion, disabled components,
entity tags and stencil state, rendering dirtiness, mixed UI/debug commands,
held UI edits, bounded history, scene replacement and injected failures midway
through editing or history restoration. Injection belongs in a temporary test
binary, not in the public debug protocol.

Verified locally: Editor build; history/persistence, readback, schema, component,
entity, reparenting, deletion, duplication, retry and observation HTTP recipes;
and visual inspection of matching saved/reloaded scene captures. A temporary
native probe verified setter/cache effects, restored IDs, tags, disabled components,
stencil state, shared and held UI commands, unsupported deletion, invalid before
states, a full component pool, the 128-command limit, play-stop invalidation,
and injected failures during both edit execution and undo. Partial effects remained
visible and history was cleared as documented. All temporary Editor/display
processes were stopped. File I/O and post-write asset-registration failures were
not injected.

Collider and RigidBody properties are restored through their adapters while stopped;
actors and shapes are created on the next Play. See [3D physics authoring](debug-server-physics.md).

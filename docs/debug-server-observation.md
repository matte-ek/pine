# Rendered observations

`POST /observe` captures the next rendered **3D perspective** Level or Game view
and returns its PNG, frame identity, camera metadata, selected entity state, and
incremental logs in one JSON response. It uses the existing localhost enablement
and five-second HTTP timeout. The main thread never waits for another frame.

## Edit, frame, observe

Successful `POST /edit`, `POST /camera`, `POST /camera/frame`, `POST /level/camera`,
`POST /level/load`, and [`POST /assets/import`](debug-server-import.md)
responses include an `observationToken` object, as do level save/save-as and
history undo/redo responses. Execution failures
that may have partially changed a batch also receive a token; validation failures
do not. Tokens contain a server `session`, `sceneGeneration`, debug mutation
`revision`, the frame at which the operation ran, and a `logsSince` cursor sampled
immediately before that operation.

Send the token from the operation you want to observe:

```json
{
  "after": {
    "session": "<copy from observationToken>",
    "sceneGeneration": 1,
    "revision": 3,
    "frame": 100,
    "logsSince": 42
  },
  "view": "level",
  "width": 800,
  "entities": ["<model entity id>", "<light entity id>"]
}
```

Copy the entire token; do not construct one from these example numbers. `view`
defaults to `level`; `game` selects the Game viewport. `width` accepts 1–4096 and
only downsizes, preserving aspect ratio. Omit it for native resolution. `entities`
accepts up to 128 existing scene entity IDs, defaulting to none. An optional
`logsSince` overrides the token's cursor—for example, use the creation token's
cursor while observing a later camera-framing token to cover the whole workflow.

`{}` is also valid: it requests a fresh view after the observation request was
accepted. It defaults to logs emitted since acceptance. Bodies are limited to
16 KiB and eight JSON nesting levels; unknown fields are rejected with HTTP 400
and an `error`/`path` response.

The response contains:

- `after`: the requested token (or the token established at acceptance).
- `frame`: `session`, monotonically increasing `id`, `sceneGeneration`, and
  `revision` (the latest debug mutation applied before this frame began).
- `viewport`: `view`, native rendered `width` and `height`.
- `camera`: camera component `id`, world `position` and `rotation`, vertical
  `fieldOfView` in degrees, clipping planes, and `viewMatrix`/`projectionMatrix`.
  Matrices are arrays of four **columns**, each containing four numbers.
- `image`: `contentType: "image/png"`, `encoding: "base64"`, output dimensions,
  and `data` containing the base64 PNG. Decode it directly to a PNG file.
- `entities`: the same entity descriptions as `GET /entity?id=...`, including
  serialized `data` and [writable `properties`](debug-server-editing.md#writable-state-readback).
- `picking`: `null` by default. With `"picking": true`, a retained capture reference
  and limits for [`POST /pick`](debug-server-picking.md). This requires stopped edit
  mode and captures model surfaces at the returned image's resolution.
- `logs`: the incremental `/logs` response described below.
- `timing`: explicit sampling phases for the camera, image, entities, and logs.

## Timing and intervening changes

An observation always waits for a frame **after its acceptance**, even if the
specified operation has already rendered. The frame includes debug mutations
through the reported revision. A token is an ordering barrier, not a saved scene
snapshot: later debug edits, UI edits, mouse navigation, or simulation updates
can supersede an operation before capture. They are included in the observed
view. The API does not freeze the editor or promise historical pixels.

Camera parameters, matrices, and viewport dimensions are copied at that
viewport's `RenderContext` callback, after camera preparation and before drawing
the color passes. Pixels are read after rendering finishes, **before ImGui and
queued debug writes**. This avoids later UI changes contaminating the metadata.
Entity state, including writable properties, is sampled at this same post-render
boundary; it describes state at that time, not a snapshot of every runtime value
used during drawing.
Logs are copied under the log mutex after capture; asynchronous messages can
arrive during rendering or PNG encoding, so logs are not an atomic scene snapshot.

HTTP 409 explicitly rejects:

- A token from another server session or a replaced scene. Every scene reset,
  including reloading the same Level, loading an empty Level, or restoring the
  play-mode snapshot, invalidates old tokens. A reset while waiting also fails.
- A hidden/inactive viewport, a missing camera or framebuffer, an invalid render
  size, or a non-perspective view. No stale framebuffer is substituted.
- A requested entity that disappears while the observation is pending.

HTTP 504 means the editor did not finish within five seconds. Timed-out pending
observations are discarded when the main thread resumes; reads have no scene
side effects. Shutdown wakes waiting requests with HTTP 503. Mutating requests now have
[explicit retry and completion rules](debug-server-requests.md), including cancellation
before execution and retained outcomes for identified requests.

`GET /viewport.png` remains the immediate PNG endpoint. Use `/observe` when frame
ordering or matching image/metadata matters.

## Incremental logs

`GET /logs` retains its existing `messages` and `totalBuffered` fields and its
most-recent-N `?limit=N` behavior. Each entry now has a monotonic `sequence`.
Responses add `oldestSequence`, `latestSequence`, `nextCursor`, `hasMore`, and
`historyLost`.

Use `GET /logs?since=<nextCursor>&limit=64` to read entries strictly after a cursor,
oldest first. With `since`, a limit pages forwards without skipping unread entries.
`nextCursor` advances to the last returned entry; follow it while `hasMore` is true.
An empty page preserves the cursor. Error and warning entries are returned along
with other severities, so clients can filter without losing cursor continuity.

Pine retains 256 entries. `historyLost: true` explicitly reports when the requested
cursor predates the retained history; the surviving entries are still returned.
Cursors are valid only within one server process. A cursor ahead of the current
history returns HTTP 409. Negative, malformed, and overflowing cursors return 400.

## Verification

Build with `cmake -S . -B build` followed by
`cmake --build build --target Editor -j4`. There is no test framework in this repo.
The reusable HTTP verification script is
[`verify-observation.py`](../Editor/src/DebugServer/Verification/verify-observation.py).
From the repository root, prepare and launch a **temporary project** (the Editor
opens an untitled level when the project has no Level asset):

```sh
observation_root=$(mktemp -d /tmp/pine-observation.XXXXXX)
observation_editor=$(realpath build/Editor/Editor)
mkdir -p "$observation_root/data/projects/observation/assets"
cp -a data/engine data/editor "$observation_root/data/"
cd "$observation_root/data"
PINE_X11=1 PINE_DEBUG_SERVER=19023 "$observation_editor" observation
```

Select the Level tab. From another terminal at the repository root, run:

```sh
python3 Editor/src/DebugServer/Verification/verify-observation.py \
  --url http://127.0.0.1:19023 --output /tmp/pine-observation-results
```

It creates a cube and a light, frames them, observes immediately, moves the cube,
and observes again without client sleeps. It checks frame ordering, camera matrix
aspect ratio, entity readback, PNG dimensions, restoration, incremental log paging,
and invalid requests. It saves captures for visual comparison. It leaves its scene
entities in that temporary Editor; close the Editor afterwards.

Also verify by switching viewport tabs, replacing the level while an observation
is queued, navigating with the mouse while pending, and timing out a read while
the editor is suspended. These lifecycle cases require control of the temporary
Editor's UI/process and are deliberately separate from the HTTP-only recipe.

Verified locally with an isolated project and virtual display: the HTTP recipe and
visual before/move/restore comparison; hidden Level and missing Game camera errors;
same empty level reload; 24 concurrent observations invalidated across replacement;
mouse navigation during pending observations; play-stop invalidation; HTTP timeout
and capture recovery; successful Game captures with matching scene-camera metadata and image dimensions;
concurrent log snapshots with retention overflow; and clean Editor shutdown.
The shutdown check completed its requests before exit, so the HTTP 503 branch
was not forced during live verification.

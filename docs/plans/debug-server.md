# Plan: Editor debug server

An HTTP server inside the Editor that exposes engine state over localhost, so an agent (or a
human with `curl`) can look at a running Pine instance without going through the ImGui UI.
Paths are relative to the repo root.

Two phases. Phase 1 is the transport plus a thin read-only endpoint set. Phase 2 is a generic
Pine-binary → JSON translator in `Core/`, which then makes the phase-1 endpoints much richer
without adding per-type code. They are deliberately separate: the translator is independently
testable against the `.passet` files already in `data/`, and landing both at once means debugging
their interaction instead of each piece.

## Settled decisions

- **HTTP:** `cpp-httplib` (single header), vendored in `third-party/cpp-httplib/` as an INTERFACE
  library alongside `material-icons` / `perlin-noise`.
- **JSON:** `nlohmann/json`, already a dependency — `EngineCli/src/Application.cpp` uses it and
  `Pine::SerializationJson` wraps it. No new dependency.
- **Images:** `stb_image_write.h` from the system `stb` package (`/usr/include/stb/`). Nothing to
  vendor, nothing to add to CMake — but **`setup-env.sh` must gain `stb` in its pacman list.** It
  installs `nlohmann-json` and not `stb`, so a machine set up by the repo's own script today would
  not have this header. Easy to miss because the dev machine already has it.
- **Image format: PNG, not JPEG.** JPEG's artifacts (edge ringing, dark-gradient blocking, chroma
  bleed) read exactly like shadow acne, banding and light leaks. The primary consumer of these
  screenshots is looking for rendering bugs, so a lossy encoder actively manufactures false ones.
- **Enablement:** bound to `127.0.0.1` only, off unless `PINE_DEBUG_SERVER` is set — matching the
  existing `PINE_X11` env-var convention (`Editor/src/Application.cpp:33`). This is an arbitrary
  control channel into the editor; cheap to gate now, annoying to retrofit.
- **Home:** `Editor/src/DebugServer/`. AGENTS.md is explicit that editor-only behavior stays behind
  the editor. Transport/queue/dispatch stay separate from endpoint bodies so moving the transport
  into `Engine/` later (for GameHost) is a re-home, not a rewrite.

## Phase 1 — transport and read-only endpoints

### The threading model (the part that matters)

Every Pine subsystem is global mutable state in an anonymous namespace with no locking, and
OpenGL is main-thread-only. httplib runs each handler on its own thread. So the invariant is:

> **The HTTP thread never touches engine state.** It parses the request, hands a job to the main
> thread, blocks on a condition variable, and serializes whatever comes back.

The obvious tool — `Threading::QueueTask(..., TaskThreadingMode::MainThread)` — is a trap here.
`PumpMainThreadTasks()` is only ever called from inside `AwaitTaskResult` / `AwaitTaskPool`
(`Engine/src/Pine/Threading/Threading.cpp:240,260`), and `Engine::Run()` never pumps. An HTTP
thread queueing a main-thread task and awaiting it would hang forever.

So: a small dedicated request queue owned by the debug server, drained once per frame from a
`RenderManager::AddRenderCallback` hook on `RenderStage::PostRender`. That is the seam every other
editor subsystem already uses (Gui, Gizmo2D/3D, IconStorage, EntitySelection, RenderHandler all
register one). No engine change.

The resulting endpoint signature is `(json request) -> json response`, already on the main thread,
which keeps adding endpoint number seven a one-file change.

Shutdown: `svr.stop()` plus joining the listener thread from `DebugServer::Shutdown()`, called from
`Editor/src/Application.cpp` before `Gui::Shutdown()`. Any handler blocked on the queue must be
released with an error rather than left waiting on a queue nobody will drain again.

### Build integration

Verified before starting: httplib 0.56.0 (22,875 lines), `stb_image_write.h` and `nlohmann/json.hpp`
compile together clean under `-std=c++17 -pthread`, with no OpenSSL/zlib defines set. No language
standard bump needed.

`httplib.h` gets included in **exactly one** `.cpp`. It is ~10k lines and both Editor and Engine use
`GLOB_RECURSE`, so leaking it into a header taxes every translation unit. Same for
`STB_IMAGE_WRITE_IMPLEMENTATION`. Link `Threads::Threads` on Editor explicitly rather than relying
on glfw/mono to drag pthreads in.

### Endpoints

| Endpoint | Source |
|---|---|
| `GET /logs` | `Pine::Log::GetLogMessages()` (`Core/Log/Log.hpp:51`) — a deque that already exists. Highest value of the set. |
| `GET /status` | play state (`PlayHandler::GetGameState()`), active level, frame time, project |
| `GET /entities` | `Entities::GetList()` + hierarchy + component **types only** — see below |
| `GET /viewport.png` | `?view=level\|game&width=640`; `RenderHandler` framebuffer + `ReadPixels` |
| `GET /stats` | `RenderingContext::Statistics` + `Performance::GetTrackedScopes()` |
| `POST /level/load` | the one mutating endpoint, to prove the round-trip |

**`/entities` stays deliberately thin in phase 1** — ids, names, hierarchy, component type list, no
field values. Phase 2 replaces hand-written field dumps wholesale, so writing them now is work we
delete.

**`POST /level/load`** refuses while play mode is running (`EditorGameState::Playing`), with a clear
error. It does *not* check for unsaved changes — deliberately out of scope for now.

### Screenshots

`IFrameBuffer::ReadPixels` already exists (`Graphics/Interfaces/IFrameBuffer.hpp:52`, GL
implementation at `Graphics/OpenGL/FrameBuffer/GLFrameBuffer.cpp:302`), and
`Editor/src/Other/EntitySelection/EntitySelection.cpp:169` is a working example of reading back an
editor framebuffer. The editor already owns both targets via `RenderHandler::GetLevelFrameBuffer()`
/ `GetGameFrameBuffer()`.

Two things to get right: GL reads bottom-up, so the rows need flipping before encoding; and the
grab must happen inside the frame callback, not from the HTTP thread.

## Phase 2 — `Core/` binary → JSON translator

### Why a fully generic translator works

`PINE_COMPACT_MODE` is `false` (`Core/Serialization/Serialization.cpp:13`), so everything on disk is
flexible mode: every field carries its `DataType` byte plus an inline length-prefixed name
(`DataHeaderFlexible`, `:68`). A walker needs no schema — all nine primitive types decode exactly
from `PrimitiveDataTypeToSize`.

The part that makes it worth building is the nesting. Every nested payload is a **complete**
serializer blob, `FileHeader` and magic included:

```
Blueprint.cpp:38   componentSerializer.Data.Write(component->SaveData());   // Light.cpp:150 -> serializer.Write()
Blueprint.cpp:40   entitySerializer.Components.AddData(componentSerializer.Write());
```

So the chain `.passet` envelope → asset payload → entity → components array → component blob →
component fields has `PINE_MAGIC` at every single level. A recursive translator that sniffs for
magic (`0x7143`) + `Version == 1` + the `FlexibleMode` flag, and requires the field walk to consume
the buffer *exactly*, decodes the whole tree with zero per-type code.

### What that unlocks

`GET /entities/{id}` returns every component's full field set for free: call `SaveData()`, translate
the blob. No hand-written per-component JSON, and it cannot drift from the real serializer because
it *is* the serializer's output — including components added later without touching the server.

Also `GET /assets/{path}`, and `EngineCli --dump foo.passet` for about five lines on top.
Note the `.passet` path goes through `File::ReadCompressed` and the `Asset` envelope
(`Assets/Asset/Asset.cpp:309`), not a raw file read.

### Known limits — accepted, not bugs

1. **`DataType::Data` is opaque by design.** When the magic sniff fails, emit
   `{"type": "data", "size": N}` plus a short hex head. That is the honest answer.
2. **`DataArrayFixed` is indistinguishable from `Data`.** Its constructor passes `DataType::Data`
   (`Serialization.cpp:321`), so the stride never reaches the file — `PINE_SERIALIZE_ARRAY_FIXED`
   data (mesh index buffers, etc.) reads back as an unknown blob. Fixing it properly means a new
   `DataType::ArrayFixed` carrying a stride byte, i.e. a format version bump. Not in this plan.
3. **The sniff must never hard-fail.** Magic + version + flag + exact-consumption makes a false
   positive near-impossible, but the fallback on *any* mismatch is "emit as opaque blob and keep
   walking", never an error. An endpoint that 500s on one odd field is useless.

### One direction only

JSON → binary is explicitly out of scope. There is no schema validation on the way in, and limit 2
means a mesh could not be round-tripped faithfully anyway. This is a debug **view**, not an editing
channel. Future mutation should go through typed endpoints calling the real setters, not by posting
blobs back. (Worth revisiting later; noted as a known want, not a gap in this plan.)

### Home

`Engine/src/Pine/Core/Serialization/Json/` — the folder already exists. It is a Core utility that
happens to have a debug server as its first consumer, not a debug-server feature.

## Verification

The repo has **no test suite** — no `enable_testing`, no `add_test`, no test directory anywhere. So
this change is not the place to introduce a framework. Verification is a clean build plus hitting
each endpoint by hand with `curl`, and eyeballing the returned PNG.

What would be worth testing if a framework existed: the phase-2 translator against the `.passet`
files already in `data/` (round-trip a known Level and assert the field names/values), and the
magic-sniff false-positive path against deliberately non-serializer blobs.

Note `CMakeLists.txt` uses `GLOB_RECURSE`, so adding these files needs a fresh CMake configure, not
just a rebuild.

## Out of scope

- JSON → binary (above).
- Unsaved-changes detection on level load.
- Any non-localhost binding, auth, or TLS.
- A `DataType::ArrayFixed` format change.
- Exposing this from GameHost.

## Open questions

- Does `/logs` need a cursor/`?since=` parameter, or is dumping the whole deque fine? The deque is
  already bounded; starting simple and adding the cursor when it actually gets annoying.
- Port number — a fixed default (e.g. 8080 is taken too often; something like 9002), or read from
  the same env var (`PINE_DEBUG_SERVER=9002`)? Leaning on the latter: one knob, enables and
  configures.

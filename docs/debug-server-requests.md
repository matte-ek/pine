# Debug request retries and completion

The localhost debug server supports retry identities for `POST /edit`,
`POST /camera`, `POST /camera/frame`, `POST /level/camera`, `POST /level/load`, and
[`POST /assets/import`](debug-server-import.md), as well as `POST /level/save`,
`POST /level/save-as`, `POST /history/undo` and `POST /history/redo`. Existing callers may
omit retry headers; their requests are not retained or deduplicated. For creation
and other writes that need reliable recovery from a lost reply, use both headers
below. Scene and camera operations still execute only on the main thread.

## Send and recover a mutation

First read `GET /requests`. It returns the server `session`, `timeoutSeconds` (5),
`retentionSeconds` (600), `maxRetainedRequests` (256), and `maxActiveRequests` (256).
The session matches the session in [observation tokens](debug-server-observation.md).
It changes when the editor restarts, but stays the same across level replacement
and play-mode scene restoration.

Choose a fresh client identity for each logical operation, such as a UUID, and
send:

```sh
curl -sS -H 'Content-Type: application/json' \
  -H 'X-Pine-Session: <session from GET /requests>' \
  -H 'Idempotency-Key: <unique operation ID>' \
  --data-binary @edit.json http://127.0.0.1:9002/edit
```

Keep the identity, session, route, query, and exact body bytes until the operation's
outcome is known. A retry within retention joins the original in-flight request
or returns its original HTTP status and JSON body. It never invokes the operation
again. Created IDs, partial-failure details, and observation tokens are preserved.
An active retry uses the **original deadline**, not another five-second allowance.

Identity keys accept 1–128 ASCII letters, digits, `.`, `_`, and `-`. Both headers
are required when either is present; malformed, empty, or repeated headers return
400. Retry headers on read endpoints, including `POST /observe`, return 400.
A mutation using a different server session returns 409 before admission.

Keys are shared by all mutation routes in a session. Reuse with a different route,
decoded query mapping, or body returns 409 without changing the original record.
Body comparison is byte-for-byte: JSON whitespace or member-order changes count
as a different payload. Query ordering and equivalent URL escaping do not; duplicate
query names are rejected. Other HTTP headers do not participate in identity.
Mutation bodies have a transport cap of 256 KiB (413); individual endpoints retain
their own validation limits.

Mutation responses add a `request` object:

```json
{
  "id": "client-operation-id",
  "session": "server-session",
  "tracked": true,
  "state": "succeeded",
  "mayHaveExecuted": true
}
```

Unidentified mutations have `tracked: false` and an empty `id`. Registry admission
failures include `state: "rejected"`, `tracked: false`, and `mayHaveExecuted: false`;
they do not create a record. Earlier HTTP header/query parsing errors return the
usual `error` response before admission. A conflicting retry rejects that attempt
only: use status to inspect the original request.

## Status and cancellation

These endpoints run on HTTP workers against the synchronized request registry.
They do not wait for the editor's main thread, although HTTP worker availability
still limits throughput.

```sh
curl -sS -H 'X-Pine-Session: <session>' \
  'http://127.0.0.1:9002/requests?id=<operation-id>'

curl -sS -X POST -H 'X-Pine-Session: <session>' \
  'http://127.0.0.1:9002/requests/cancel?id=<operation-id>'
```

Status returns HTTP 200 with `request` and, once terminal, a
`result: {"status": <original HTTP status>, "body": <original JSON response>}`.
It returns 200 even when the stored result is an error. Control requests accept
only `?id=` and the session header, with no body or idempotency header.

| State | Meaning | `mayHaveExecuted` |
|---|---|---|
| `pending` | Admitted; handler has not started. | false |
| `running` | Main thread has started validation/execution; outcome is not yet known. | true |
| `succeeded` | Handler finished successfully. | true |
| `rejected` | Handler rejected validation or a precondition before mutation. | false |
| `failed` | Execution failed or threw; inspect the retained response for partial changes. | true |
| `cancelled` | Cancelled, expired, or shut down before execution. | false |
| `unknown` | No retained record; never admitted and expired cannot be distinguished. | true |

`mayHaveExecuted` is conservative: running includes validation, and a failed
operation may have made no changes. A failed batch still reports its completed
operations; no automatic rollback or undo guarantee is added.

Cancelling pending work returns its terminal snapshot and wakes any waiting
mutation callers with a retained 409 response. Repeating cancellation returns the
same snapshot. Cancelling completed work returns its existing result and has no
scene effect. Running work returns 409 with its current request state and continues
until it finishes; cancellation never interrupts a component setter or rolls back
an operation.

Unknown IDs return 404 with `state: "unknown"`. Looking up or cancelling an ID from
a different session returns 409 with an unknown outcome and `currentSession`.
Neither response proves that the old operation did not run. Cancellation cannot
reserve an unknown identity or cancel an unidentified request.

## Deadlines, disconnects, and retention

The five-second deadline starts when a request is admitted to the main-thread
queue. The queue checks it again immediately before starting each handler,
including requests already moved into a frame's batch.

- If the deadline expires before the handler starts, the request becomes cancelled
  with a retained 504 response. It cannot execute when the editor resumes. This
  also applies to callers without retry identities.
- If a mutation has started, its HTTP caller receives 504 with `state: "running"`
  and `mayHaveExecuted: true`. The handler continues. Identified requests retain
  the eventual result; poll status or retry with the same identity and payload.
- Timed-out reads are discarded, including deferred observations. A read already
  computing may finish, but its result is discarded and its continuation is not
  resumed. Reads have no scene side effects.
- A client disconnect is **not** cancellation. The server may finish the request
  before detecting that the reply cannot be delivered. Retry with the same identity.
- Shutdown releases queued and deferred waiters with 503. A mutation already
  executing finishes before the main thread can shut down. Records are in memory;
  after a crash or restart, recover scene state before deciding on another write.

Terminal results are retained for **ten minutes after completion**, including
rejected and cancelled requests. Retries and lookups do not extend retention.
In-flight records are not evicted. At 256 retained identities, new identified work
returns 503 before admission. At 256 active queued/running/deferred requests, all new
work returns 503 before admission. Existing identities
remain queryable and replayable at capacity. Unexpired results are never evicted
to make room. Expired records are removed lazily on subsequent registry operations.

The deduplication guarantee ends when retention expires. An expired key submitted
again can be admitted as new work. To recover a lost reply safely, retry within
ten minutes of the **initial send**, keep the original session, and never assign
that key to another logical operation. After that window or an unknown status,
inspect the scene and reconcile the intended result before issuing another write.

Records survive scene replacement. Retrying an old creation after loading a level
returns its old result without recreating anything. Returned IDs may therefore be
stale, and its observation token will be rejected in the new scene generation.
Replaying a load similarly does not load the level again. Clients needing a fresh
operation must explicitly choose a new identity.

## Verification

Reconfigure and build with `cmake -S . -B build` and
`cmake --build build --target Editor -j4`. Use the temporary-project launch recipe
in [rendered observations](debug-server-observation.md#verification), substituting
port 19024. On a headless Linux machine, launch through `xvfb-run` and set
`ALSOFT_DRIVERS=null` if no audio device is available. Open the Level viewport.

Run the reusable HTTP recipe from the repository root:

```sh
python3 Editor/src/DebugServer/Verification/verify-requests.py \
  --url http://127.0.0.1:19024
python3 Editor/src/DebugServer/Verification/verify-observation.py \
  --url http://127.0.0.1:19024 --output /tmp/pine-retry-observations
```

The retry recipe checks eight concurrent copies of one creation, a disconnected
reply, retained validation failures, path/query/body conflicts, status, cancellation
of completed work, same-batch and existing-entity references, composed Light patches,
camera read/restore persistence over rendered frames, and all eight framed bounds
corners against the viewport and clipping planes. Captures use observation barriers.

Optionally add `--level <loaded-Level-path>` to the retry recipe. Use an empty Level
saved through the temporary editor: this check deliberately replaces the scene,
replays old creations and loads, and checks stale tokens and capture ordering.
Add `--fill-retention` to verify capacity rejection and preservation of earlier
results; this fills the ten-minute registry, so run it last or restart the temporary
editor before another identified-mutation verification run.

For lifecycle verification, pause the main thread in a debugger while leaving HTTP
workers running. Queue an identified mutation and verify status is pending; cancel
it, resume, and verify no entity appeared. Repeat without cancellation for more than
five seconds: the request must return 504/cancelled and remain unexecuted after
resumption. Break inside a mutation handler after dispatch has marked it running:
status must show running, cancellation must return 409, and a timeout must report
possible execution. Resume and verify a single completed result is available through
status and retry. A process-wide suspension also stops HTTP workers, so it cannot
verify that status remains responsive during a main-thread stall.

Verified locally: Editor build; both HTTP recipes with an isolated project and
virtual display, including scene replacement and retention capacity; and a temporary
native queue probe using the production Requests implementation to control pending/
running work, cancellation, five-second deadlines, retained failures/exceptions,
and shutdown wakeups. The ten-minute expiry boundary was not timed in that run.
No test framework was added. Close the temporary Editor after verification.

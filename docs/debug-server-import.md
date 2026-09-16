# Asset import through the debug server

`POST /assets/import` imports one local source file using the editor's existing
import context: copy the source into `content/`, write the `.passet` under
`assets/`, load/register the asset, and refresh the asset browser. Execution is
synchronous on the main thread; large imports can block frames.

## Request

```json
{
  "source": "/tmp/generated/prop.glb",
  "directory": "models/props",
  "overwrite": false
}
```

- `source` is a required regular file accessible to the Editor. Absolute paths
  are recommended; relative paths resolve from its working directory (normally
  `data/`). The source does not need to be inside the project.
- `directory` defaults to the project asset root; an empty string also selects
  the root. Use a lowercase project-relative directory without a trailing slash,
  `.` or `..` components, or symbolic links. Missing directories are created.
  The destination does not depend on the asset browser selection.
- The source filename supplies the asset name using the existing importer's
  path rules. This example produces virtual path `models/props/prop`, file
  `assets/models/props/prop.passet`, and source copy `content/models-props-prop.glb`.
- `overwrite` defaults to false. Set true to update a loaded asset of the same
  type at the destination, preserving its ID. An existing unloaded `.passet` or
  an asset of another type remains a conflict.

Registered source file types and default/proposed import settings are inherited
from Pine's importer. Updates retain existing settings. Directories, multiple
source files, custom settings and uploads are not accepted. Bodies are limited
to 16 KiB and eight JSON nesting levels; unknown fields and query parameters
are rejected. Stop play mode and finish or cancel any pending UI import dialog
before calling this endpoint.

## Response

HTTP 200 means all queued entries completed. `imports` lists the main asset first,
then any dependencies queued by the importer:

```json
{
  "imports": [{
    "source": "/tmp/generated/prop.glb",
    "action": "create",
    "status": "imported",
    "type": "Model",
    "id": "<asset UID>",
    "path": "models/props/prop",
    "file": "projects/example/assets/models/props/prop.passet",
    "sources": ["projects/example/content/models-props-prop.glb"]
  }]
}
```

The server adds `request` and `observationToken`, as with other mutations. The
asset ID is immediately usable in a ModelRenderer edit or `GET /asset?id=...`.
Pass the token to `/observe` to capture a frame after re-importing a visible model.

HTTP 400/409 rejects validation or preconditions before execution. HTTP 500 means
execution failed and can have partial effects: source copies, dependency assets,
or changes to an existing asset may already exist. The response includes
`imports`, an error, `failedOperationMayHaveChangedState: true`, and an observation
token. Failed entries omit the successful asset fields; inspect `/logs` for
importer diagnostics. There is no rollback or asset-import undo step.

Dependencies retain the existing importer's naming, source handling, warnings
and update policy. `overwrite` guards the main destination only. Successful import
does not guarantee that every feature of the source format is supported.

## Retries and long imports

Use `X-Pine-Session` from `GET /requests` and a unique `Idempotency-Key`. A retry
with those headers and the exact payload returns the original result; it does
not re-read a source file that has since changed. Use a new key for a revised file.

The existing five-second HTTP deadline applies. If it expires after import starts,
HTTP 504 reports request state `running`, and the import continues. Poll
`GET /requests?id=<key>` with the session header for the eventual result. There
is no separate job or progress API. See [request lifecycle](debug-server-requests.md).

## Verification

Build the Editor and use the [temporary-project launch recipe](debug-server-observation.md#verification)
on port 19029. Open the Level viewport, then run:

```sh
python3 Editor/src/DebugServer/Verification/verify-import.py \
  --data /tmp/<temporary-project>/data \
  --model /absolute/path/to/a/self-contained-model.glb
```

The recipe copies the GLB before modifying it. It checks external-source copying,
live registration, rendering, re-import with stable identity and updated bounds,
concurrent retry deduplication, validation, conflicts, importing a content file
onto itself, and retained failures. It writes captures and fixtures to a temporary
directory and prints the asset path/ID for checking persistence after restarting
the temporary Editor.

Verified locally with a Blender-generated GLB and an isolated project: the recipe
above, visual before/after captures, explicit empty-directory imports, and asset
identity/geometry persistence after restarting the Editor. The inherited long-running
request timeout and UI-dialog/play-mode rejection branches were not forced in this run.

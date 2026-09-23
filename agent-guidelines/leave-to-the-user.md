# Actions to leave to the user

Some steps belong to the user. Do the development work and report it, so the user decides when a change is tried out, recorded, and released.

Unless the user asks for the action in the current request, or the project's instructions say otherwise:

- **Version control:** do not push, do not create branches, tags, or pull requests. Leave the work in the working tree and describe it. Offer a commit.
- **Deployment:** do not release, publish, or promote anything to an environment, and do not run a pipeline, migration, or infrastructure command that would.

## Running the application is fine here

The Editor carries a debug server (see [`docs/debug-server.md`](../docs/debug-server.md)) whose
whole purpose is to let an agent start the editor, look at engine state and inspect the viewport.

Ssee [`docs/editor.md`](../docs/editor.md#running-it-headlessly)), and shut down anything you started once you are done rather than leaving processes running in the background.
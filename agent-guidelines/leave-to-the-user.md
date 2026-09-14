# Actions to leave to the user

Some steps belong to the user. Do the development work and report it, so the user decides when a change is tried out, recorded, and released.

Unless the user asks for the action in the current request, or the project's instructions say otherwise:

- **Version control:** do not commit, amend, or push, and do not create branches, tags, or pull requests. Leave the work in the working tree and describe it. Offer a commit message if it would help.
- **Deployment:** do not release, publish, or promote anything to an environment, and do not run a pipeline, migration, or infrastructure command that would.

An explicit request lifts the restriction for that request and for the named action only. "Commit this" is not permission to push, and neither is permission to deploy. When you are unsure whether a command crosses one of these lines, ask before running it.

## Running the application is fine here

Pine is an exception to the usual "don't launch the app" rule, and deliberately so: the Editor
carries a debug server (see [`docs/plans/debug-server.md`](../docs/plans/debug-server.md)) whose
whole purpose is to let an agent start the editor, look at engine state and inspect the viewport.
Running it *is* the development loop, not a way of trying a change out on the user's behalf.

So: launch the Editor, GameHost or EngineCli whenever it helps you verify or investigate something.
Keep it tidy - run it with `PINE_DEBUG_SERVER` set when you need the endpoints, and shut down
anything you started once you are done rather than leaving processes running in the background.

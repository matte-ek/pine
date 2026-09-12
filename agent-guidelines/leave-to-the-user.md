# Actions to leave to the user

Some steps belong to the user. Do the development work and report it, so the user decides when a change is tried out, recorded, and released.

Unless the user asks for the action in the current request, or the project's instructions say otherwise:

- **Running the application:** do not launch or run the application to try a change out. Building the project and running its tests is yours; starting the app, a server, or a long-running process is not.
- **Version control:** do not commit, amend, or push, and do not create branches, tags, or pull requests. Leave the work in the working tree and describe it. Offer a commit message if it would help.
- **Deployment:** do not release, publish, or promote anything to an environment, and do not run a pipeline, migration, or infrastructure command that would.

An explicit request lifts the restriction for that request and for the named action only. "Commit this" is not permission to push, and neither is permission to deploy. When you are unsure whether a command crosses one of these lines, ask before running it.

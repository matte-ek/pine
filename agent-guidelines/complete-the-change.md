# Complete the change

A working method is not a finished feature. Before reporting code work as done, trace the change through the integration points the project actually uses, and ask: **What would a developer most likely forget when implementing this?**

## Trace the feature end to end

Where they apply to the change, check that:

- **Registrations** exist, so the service, handler, route, command, or component is reachable through its intended path. Check for convention-based discovery before concluding that a registration is missing.
- **Configuration** is complete, including required settings, defaults, validation, and bindings.
- **Permissions** and authorization checks are applied at the entry points that need them.
- **Related updates** are in place, including affected mappings, persistence changes, callers, and tests.

Let the project and the feature decide which of these apply. Not every change needs every one.

## Finish patterns you started

- When a pattern applies across comparable cases, apply it to all of them. If seven of eight comparable endpoints require a permission, the eighth needs a reason to differ.
- Update paired and mirrored operations together. A mapping changed in one direction usually needs the reverse direction as well.
- Check related branches, variants, and consumers so the change is not left partially applied.

Establish the applicable pattern from comparable cases rather than assuming every difference is wrong. When a difference looks deliberate but unexplained, raise it with the user instead of quietly changing it.

## Verify what you changed

Build the project and run the existing tests that cover the area you touched. Report the outcome, including failures you did not fix and anything you could not run. Never present unverified work as verified.

Match the verification to the change. Prefer the narrowest run that would actually catch a mistake over the full suite, and re-run what your change could plausibly have broken.

Fix the tests your own change broke. A failure you caused is part of the work, not a separate task to hand back. Correct the code, or update the test when the requested change deliberately changed the expected behavior, and say which one you did. Do not weaken, skip, or delete a test to make it pass.

Failures that were already present before your change are not yours to fix. Report them and leave them alone unless the user asks.

Add tests only when the project already has them. Follow the existing tests' structure, naming, and assertion style, and cover the behavior you added or changed. In a project with no tests, do not introduce a test framework as part of another change; say what you would test instead.

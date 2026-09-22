# Consistency with nearby code

Make new code feel like it belongs in the project. Before editing, read the target file and a few relevant neighboring implementations, including analogous features when useful.

## Follow the relevant conventions

- Check applicable project instructions, formatter settings, and lint rules before choosing a style.
- Match nearby naming, indentation, brace placement, blank-line grouping, declaration order, and preferred language constructs.
- Follow established approaches to error handling, asynchronous work, logging, dependencies, and tests where relevant to the change.
- Reuse existing helpers and domain concepts when they fit. Avoid creating a competing way to do the same thing without a concrete benefit.
- If the repository contains several styles, follow the explicit project rules and the most relevant maintained code in the area being changed. Avoid treating an unrelated file or isolated exception as the standard.

## Improve clarity within the existing style

Respect local conventions while keeping the new logic straightforward and well spaced. Existing dense code is not a reason to introduce more dense code, and a known bug or unsafe pattern should not be copied for consistency. Likewise, heavily commented nearby code is not a comment density to match; follow [Readable code](readable-code.md) for what earns a comment.

Consistency does not make the existing structure immutable. Refactor nearby code when doing so makes the requested feature cleaner and easier to maintain, keeping the resulting code coherent with the project's conventions.

Keep formatting changes and refactoring focused on the requested work. When a change departs from an established pattern, give the advance notice described in [Design for likely changes](design-for-change.md).

For a new area with no established conventions, use simple, idiomatic patterns suited to the project's language and tooling. Keep those choices consistent throughout the addition.

Before finishing, compare the change with its surroundings. It should read as a natural continuation of the codebase and be easy for its maintainers to review.

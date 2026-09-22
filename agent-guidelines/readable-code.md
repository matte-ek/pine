# Readable code

Optimize code for the people who will read, review, debug, and maintain it. Correctness is essential; readability is also part of a finished implementation. Do not compress code to save tokens or minimize line count.

## Keep the logic straightforward

- Prefer the simplest familiar pattern that handles the requirement correctly. Add complexity only when it solves a concrete problem.
- Use descriptive names that explain purpose in the domain. Introduce intermediate variables when naming a value or condition makes the next step easier to understand.
- Prefer explicit control flow when nested expressions, chained operations, or clever language features would require a reader to mentally unpack the logic.
- Use guard clauses where they make the main path easier to follow. Keep validation, decisions, and side effects easy to identify.
- Keep functions focused on a coherent task. Extract a helper when it gives a meaningful operation a useful name or isolates complexity. Avoid scattering a simple sequence across many tiny helpers.
- Concise expressions are welcome when immediately understandable, such as a simple property or a straightforward query. Judge readability, not length.

## Keep comments proportionate

Use comments to explain intent, constraints, or non-obvious decisions. Prefer clearer code over comments that merely translate the syntax.

- A comment describes the code as it is now. What it replaced, the bug it fixed, and alternatives you considered belong in the commit message or your report to the user.
- Give each rationale one home, at the declaration it governs. Elsewhere, point to it ("see `ShadowAtlas::Reserve`") rather than restating it.
- Keep comments short: usually a line or two. Save a paragraph for a genuinely non-obvious constraint.
- Explain the choice that was made. Do not argue against alternatives the code does not suggest.
- The tradeoffs and decisions other modules ask you to make visible go to the user, not into code comments.

## Give code paragraph structure

Use blank lines to separate changes in purpose: validating input, loading data, computing a result, updating state, and returning a response. Keep closely related statements together within each group.

Do not write long, uninterrupted walls of code. Do not insert blank lines mechanically between every statement either. Let the spacing show the structure of the work, using the project's formatting conventions.

For example, this C# fragment groups work into readable steps:

```csharp
if (player == null)
{
    return;
}

var queue = lobby.HostQueue;
var currentPosition = queue.IndexOf(player);

if (currentPosition < 0)
{
    return;
}

queue.RemoveAt(currentPosition);
queue.Add(player);

lobby.SendMessage($"{player.Name} moved to the end of the queue.");
```

Before finishing, read the changed code as a coworker encountering it for the first time. Simplify anything that makes them decode several ideas at once.

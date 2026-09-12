# Design for likely changes

Implement the current requirement with the most probable next steps in mind. Before committing to a design, ask: **Does implementing feature X this way make a likely next step Y unnecessarily difficult or impossible?**

## Consider concrete next steps

- Ground likely changes in the request, existing domain, nearby features, or documented plans. Do not treat every imaginable extension as a requirement.
- Look for assumptions that would be expensive to undo, especially in public interfaces, stored data, external integrations, and ownership of state.
- Keep a changeable policy or assumption in one clear place when scattering it would make future changes harder.
- Preserve useful boundaries between responsibilities. Reuse the project's existing boundaries when they fit.
- Prefer a small, clear implementation that can be extended locally. A little duplication may be easier to change than an abstraction that couples unrelated behavior.

For example, if a feature currently selects one item but a documented next step will allow several, consider whether the storage format and public interface would force a disruptive migration. Keep today's behavior focused on one item while making that tradeoff deliberately.

## Adapt existing code when it improves the result

Treat existing code as changeable. When its current structure makes a new feature awkward, consider refactoring that structure as part of the implementation. Prefer a focused change to existing code when it produces a simpler, cleaner, more maintainable result than forcing the feature into the current design with workarounds, duplicated logic, or special cases.

Evaluate the clarity of the resulting code as a whole. Minimizing the number of existing lines changed is not a goal in itself. Keep refactoring connected to the feature, preserve existing behavior unless the requested change calls for otherwise, and check affected callers and tests.

Tell the user before you edit when a change meets any of these conditions:

- It alters existing behavior or a public interface.
- It removes, replaces, or relocates an established pattern.
- It touches code outside the surface of the feature being implemented.

Name the parts you propose to change, why the change helps, and the effects on existing behavior or interfaces. Give this notice before editing rather than only describing the change afterward, and keep it proportionate to the change.

Routine changes within the agreed scope need no advance notice; make them and report what you changed. Discuss material changes to the goal or scope with the user first.

## Keep the scope grounded

Build what today's requirement needs. Do not implement future features, add unused configuration, or build a general framework because it might be useful later; introduce an abstraction only when it addresses a current need or removes a concrete, costly obstacle to a probable next step.

Explain a tradeoff when it is meaningful: the assumption you made, the likely change it affects, and what would need to change later. Routine, reversible choices need no explanation.

Before finishing, check that the implementation meets today's requirement and that its important assumptions are visible and reasonably easy to revisit.

# Code review and audit

## Activation

Apply this module **only when the user explicitly requests a code review or audit**. It does not apply to general prompts, ordinary development, routine checks of your own changes, or writing review instructions. If the user requests both development and review, apply this module during the requested review phase. Keep the review within the requested scope, using surrounding code as evidence.

## Review mindset

Look beyond bugs. Actively search for inconsistencies, partial implementations, and deviations from surrounding patterns. Throughout the review, ask: **What would a developer most likely forget when implementing this feature?**

When something differs from comparable code and the reason is unclear, prefer asking **"Is this intentional?"** over silently dismissing it. An observation can be worth raising even when it is not a proven defect. Explain the difference you found and distinguish it from your interpretation.

## Pattern consistency

- Compare the reviewed code with similar existing implementations and relevant callers. Establish the applicable pattern from comparable cases, rather than assuming every difference is wrong.
- Look for a pattern applied repeatedly but missing in one location. For example, application permissions added to seven of eight comparable endpoints warrant checking the eighth.
- Check paired or mirrored operations. For example, a DTO mapping updated in one direction may also require an update in the reverse direction.
- Check related branches, variants, and consumers for changes that were applied only partially.

## Feature completion

Trace the feature through the relevant integration points. A working method alone does not establish that the feature is fully connected. Where applicable, check:

- **Registrations:** Is the service, handler, route, command, or other component registered and reachable through the intended path?
- **Configuration:** Are required settings, defaults, validation, and configuration bindings present and consistent?
- **Permissions:** Are the required application permissions or authorization checks applied at the appropriate entry points?
- **Related updates:** Do affected mappings, persistence changes, callers, and tests account for the new behavior?

Use the project and feature to determine which checks apply. Do not assume every feature needs every integration point, or that an omitted registration is missing before checking convention-based discovery.

## Outlier detection

Compare values and handling with the surrounding data and equivalent code paths. Look for unexpected differences in:

- Naming conventions and capitalization.
- Language used in labels, messages, or other related text.
- Formatting of code, strings, and data.
- Null handling, including defaults and missing-value behavior.
- Enum handling, including omitted cases, conversions, and fallback behavior.
- Ordering, including sorting, processing order, and presentation order.

Check whether the context explains each difference. If intent remains unclear, raise a question rather than treating the difference as either automatically correct or definitely broken.

## Classify and report findings

Use these labels for each finding:

| Classification | Use for |
| --- | --- |
| **HIGH CONFIDENCE** | A very likely bug or broken behavior supported by concrete evidence. |
| **MEDIUM CONFIDENCE** | A suspicious inconsistency, missing pattern implementation, or unusual omission with supporting evidence but unresolved intent or impact. |
| **LOW CONFIDENCE / QUESTION** | A difference from surrounding code that may be intentional and needs confirmation. |

Confidence describes certainty in the finding, not the severity of its consequences. Explain impact separately when known; do not inflate confidence because a hypothetical consequence would be serious.

For each finding, include the location, the observed behavior or difference, the relevant comparison or evidence, and the likely consequence or open question. For uncertain findings, ask a specific question such as: "The other seven endpoints require this permission, but this one does not. Is anonymous access intentional here?"

Lead with the most consequential supported findings. Keep tentative observations clearly labeled, but do not omit them solely because they need confirmation. Prefer asking over leaving a concrete, unexplained inconsistency unmentioned. Group repeated instances of the same issue to keep the review useful.

State any meaningful limits on what you examined or verified. If there are no findings, say so without claiming that unexamined behavior is correct. A review request alone calls for findings and questions; make fixes when the user also asks for them.

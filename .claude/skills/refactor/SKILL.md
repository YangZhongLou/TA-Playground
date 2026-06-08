---
name: refactor
description: Refactoring skill. Invoke when restructuring code without changing external behavior — simplifying, deduplicating, or improving design.
metadata:
  type: skill
  trigger: manual
---

# Refactor

## Principles

- **Behavior-preserving.** Refactoring changes structure, not output. If a test breaks and it's not the test's fault, you changed behavior.
- **Small steps.** Each step leaves the code working. No "break everything then fix" — that's a rewrite.
- **Tests first.** Green suite before starting. If the code has no tests, add characterization tests first.
- **Measurable goal.** Speed, readability, extensibility, or consistency. Not "make it better."

## When to Refactor

| Signal | Action |
| --- | --- |
| Same logic in 3+ places | Extract to one function/module |
| Function >50 lines with multiple concerns | Split by responsibility |
| Deep nesting (>3 levels) | Early return, extract condition, flatten |
| Name doesn't match what it does | Rename |
| Class/module has >1 reason to change | Split by responsibility |
| Changing one thing requires touching 5+ files | Consolidate or introduce indirection |
| Can't write a unit test without 10 mocks | Decouple dependencies |

## When NOT to Refactor

- **No tests.** Add tests first. Refactoring untested code is bug farming.
- **During a feature.** One or the other. Never both.
- **Close to release.** Risk > reward. Add a ticket for the next cycle.
- **"Just in case."** YAGNI. Refactor when the pain is real, not hypothetical.

## Catalog

### Composing Methods

| Refactor | When |
| --- | --- |
| Extract Function | A block of code can be named as a clear purpose |
| Inline Function | The function body is as clear as the name |
| Extract Variable | A complex expression needs a name |
| Replace Temp with Query | A temp var is assigned once from a query |

### Moving Features

| Refactor | When |
| --- | --- |
| Move Function | A function uses more from another module than its own |
| Move Field | A field is used more by another class |
| Extract Class | A class has two distinct subsets of behavior |
| Inline Class | A class does too little to justify its existence |

### Simplifying Conditionals

| Refactor | When |
| --- | --- |
| Decompose Conditional | Complex `if` condition — extract to named function |
| Consolidate Conditional | Same action from different conditions — combine |
| Replace Nested with Guard | Deeply nested `if` — flatten with early return |
| Replace Conditional with Polymorphism | Type-based switch/if chain — use subclass/interface |
| Introduce Null Object | Repeated null checks — provide default behavior |

### Data & API

| Refactor | When |
| --- | --- |
| Encapsulate Field | Public field → getter/setter for controlled access |
| Replace Primitive with Object | Simple type grows behavior (string → Name class) |
| Parameterize Function | Two functions differ only by a constant → one with param |
| Split Phase | One function does transform + side effect → two functions |

## Process

1. **Identify.** What's the smell? Name it specifically: "God class", "shotgun surgery", "primitive obsession".
2. **Cover.** Add tests for the code being changed. Characterization tests if none exist.
3. **Refactor.** One operation at a time. Run tests after each step.
4. **Review.** Self-review the diff. Any behavior change that's not a test fix is a bug.
5. **Repeat.** Until the smell is gone or the next step has diminishing returns.

## Anti-Patterns

- **Refactor + feature same PR.** Reviewer can't tell what's structural vs behavioral.
- **Big bang rename.** Rename half the codebase in one commit. Makes blame useless.
- **Over-abstraction.** Extracting every 3-line block. If it's not reused and not complex, leave it.
- **Framework churn.** "Let's migrate to X" is not a refactor. It ships new bugs and new dependencies.
- **Perfect code.** There's always a better way. Stop when the code is clear and correct.

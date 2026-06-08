---
name: debugging
description: Invoke when investigating bugs, crashes, or unexpected behavior. Systematic debugging methodology.
metadata:
  type: skill
  trigger: manual
---

# Debugging

## Process

1. **Reproduce.** Get a reliable repro first. No repro → no confidence the fix works.
2. **Isolate.** Bisect code, minimize inputs, eliminate variables. Find the smallest case that triggers the bug.
3. **Diagnose.** Find the root cause, not the symptom. Ask "why" until you reach the origin.
4. **Fix.** One bug per commit. Fix the cause, don't patch the symptom.
5. **Prevent.** Add a regression test that fails before the fix, passes after.

## Root Cause Analysis

| Question | What it reveals |
| --- | --- |
| When did this start? | Git bisect, deploy history, config change |
| What inputs trigger it? | Boundary values, empty, null, special chars, race window |
| Is it deterministic? | Timing bug, uninitialized state, hash order |
| What changed recently? | Dependencies, schema, config, traffic pattern |
| Does it happen elsewhere? | Same pattern in other code paths |

## Techniques

- **Rubber duck.** Explain the code line-by-line to an imaginary listener. Catches half the bugs.
- **Bisect.** `git bisect` for regressions. Binary search over inputs or config changes.
- **Minimize.** Delete code until only the bug-bearing path remains. Add back only what's needed to trigger it.
- **Instrument.** Log inputs, state transitions, and return values at the problem boundary. Remove logs after fixing.
- **Assert invariants.** Add temporary assertions to catch state corruption early.
- **Compare.** Find a similar working code path and diff the behavior step by step.

## Anti-Patterns

- **Random changes.** "Maybe this fixes it" without understanding why. Wastes time, adds noise.
- **Fix + refactor same commit.** Reviewer can't tell which change was the actual fix.
- **Silent fixes.** Bug "went away" but you don't know why. It'll come back.
- **Fix without test.** Same bug will regress within weeks.
- **Stack overflow copy-paste.** A solution for a different codebase in a different context. Understand first.

## Common Bug Categories

| Category | First places to look |
| --- | --- |
| Off-by-one | Loop bounds, index math, slice/array edge |
| Null/undefined | Optional chain missing, async result not awaited, map lookup |
| Race condition | Shared state without lock, async without await, goroutine without sync |
| State corruption | Mutation where copy expected, stale closure, dirty cache |
| Type coercion | `==` vs `===`, string+number, falsy values (`0`, `""`, `false`) |
| Resource leak | Missing close/defer, dangling event listener, unhandled rejection |
| Infinite loop | Termination condition never met, recursion without base case |

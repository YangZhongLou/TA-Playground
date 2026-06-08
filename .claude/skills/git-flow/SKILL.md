---
name: git-flow
description: Invoke when starting new work, preparing to merge, or managing branches.
metadata:
  type: skill
  trigger: manual
---

# Git Flow

## Workflow

```text
main ←── rebase ── feature/x ── commits ── QA
  ↑                                           │
  └──────── merge ────────────────────────────┘
                    then push
```

1. **Branch.** `git checkout -b feature/<name>` from latest `main`.
2. **Commit.** Small, logical commits. Present tense, imperative.
3. **QA.** Run tests, lint, type-check. Verify in the real app. Only proceed if green.
4. **Rebase.** `git fetch origin && git rebase origin/main`. Resolve conflicts per commit.
5. **Merge.** `git checkout main && git merge --ff-only feature/<name>`.
6. **Push.** `git push origin main`.
7. **Clean.** `git branch -d feature/<name>`.

## Rules

- **Never commit directly to main.** All work starts on a branch.
- **QA gate before rebase.** Tests must pass, lint clean, app runs. No skipping.
- **Rebase, don't merge main into feature.** `git rebase origin/main`, never `git merge main` into your branch.
- **Squash only if the commit history is noisy.** Otherwise preserve logical commits.
- **Force push only on feature branches.** `git push --force-with-lease`, never on main.
- **Pull before rebase.** `git fetch origin` first. Stale tracking branches cause bad rebases.

## Branch Naming

| Prefix | Use |
| --- | --- |
| `feature/` | New functionality |
| `fix/` | Bug fixes |
| `refactor/` | Structural changes, no behavior change |
| `docs/` | Documentation only |
| `chore/` | CI, deps, config |

## Commit Messages

```text
<imperative verb> <short description>

<optional body: why, not what>
```

- 50 chars max subject. Capitalize, no period.
- Body wraps at 72 chars. Explain why, not what.
- Blank line between subject and body.

## Example

```text
git checkout -b feature/user-auth
# ... make changes ...
git add -A
git commit -m "Add user authentication middleware"
# ... more commits ...
# === QA gate ===
npm test && npm run lint
# === pass, proceed ===
git fetch origin
git rebase origin/main
# resolve conflicts if any
git checkout main
git merge --ff-only feature/user-auth
git push origin main
git branch -d feature/user-auth
```

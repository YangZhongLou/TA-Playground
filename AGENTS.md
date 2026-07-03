# TA-Playground

Personal skill hub. Skills in `.claude/skills/`, agents in `.claude/agents/`, commands in `.claude/commands/`.

> Kimi CLI note:
> - skills are migrated to `~/.kimi-code/skills/` with YAML frontmatter removed.
> - markdown lint hook is migrated to `.kimi/hooks/md-lint.py` and wired via `~/.kimi-code/config.toml`.
> - Agents and slash commands are not available; their behavior is folded into this AGENTS.md and the loaded skills.

## Behavioral Guidelines

> Tradeoff: bias toward caution over speed. For trivial tasks, use judgment.

1. **Think Before Coding** — State assumptions explicitly. If multiple interpretations exist,
   present them. Push back on unnecessarily complex approaches. If something is unclear,
   stop and name what's confusing. Don't hide confusion. Surface tradeoffs.

2. **Simplicity First** — Minimum code that solves the problem. No features beyond what was asked.
   No unnecessary abstractions. No single-use code dressed up for flexibility.
   No error handling for impossible scenarios. If 200 lines could be 50, rewrite it.

3. **Surgical Changes** — Touch only what you must. Don't improve adjacent code or formatting.
   Match existing style. Only remove imports/variables/functions that YOUR changes made unused.
   Every changed line should trace directly to the user's request.

4. **Goal-Driven Execution** — Turn tasks into verifiable goals (e.g., "Add validation" →
   "Write tests for invalid inputs, then make them pass"). For multi-step tasks, state a brief
   plan with verification checks per step. Loop until verified.

## Agents

| Agent | Command | What it does |
| --- | --- | --- |
| architect | `/architect` | System design, architecture decisions, trade-off analysis, API design |
| code-reviewer | `/code-reviewer` | Review code for correctness, safety, clarity, and completeness |
| qa-engineer | `/qa-engineer` | Test writing, strategy, case design, TDD, bug reporting, quality gates |
| programmer | `/programmer` | Write code, style, language guides (TS/JS/Python/Go/Rust/Java/SQL/Shell) |
| project-manager | `/project-manager` | Milestone-based project planning, task breakdown, progress tracking |
| technical-artist | `/technical-artist` | UE material, shader, visual effects prototyping |

## Skills

| Skill | Command | What it does |
| --- | --- | --- |
| git-flow | `/git-flow` | Branch → commit → rebase → merge workflow |
| markdown-writer | `/markdown-writer` | Structured markdown writing with mandatory self-review |
| debugging | `/debugging` | Systematic debugging — reproduce, isolate, root cause, fix, regression test |
| refactor | `/refactor` | Restructure code without changing behavior — simplify, deduplicate, improve design |
| skill-reviewer | `/skill-reviewer` | Review SKILL.md files for structure, clarity, completeness, quality |
| ta-jade | `/ta-jade` | Jade/玉石 material — subsurface scattering, internal veining, color variants |

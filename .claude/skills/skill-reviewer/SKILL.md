---
name: skill-reviewer
description: Invoke when creating or updating any SKILL.md file.
metadata:
  type: skill
  trigger: manual
---

# Skill Reviewer

Review `SKILL.md` files against a quality checklist. Report issues by severity with suggested fixes.

## Critical

| Rule | Check |
| --- | --- |
| SR001 | Missing frontmatter (`name`, `description`, `metadata`) |
| SR002 | No h1 heading after frontmatter |
| SR003 | `name` in frontmatter doesn't match directory name |
| SR004 | `description` is generic or unactionable ("a skill for X") |
| SR005 | Missing `metadata.type: skill` |

## Warning

| Rule | Check |
| --- | --- |
| SR010 | No top-level structure (missing h2 sections) |
| SR011 | Heading hierarchy skips a level (h1 → h3) |
| SR012 | Section content is too thin (<3 lines or one sentence) |
| SR013 | Skill has no actionable rules, checklists, or templates |
| SR014 | Overlap with another skill (repeated rules/guidance) |
| SR015 | Language is vague: "should", "consider", "maybe", "could" → replace with imperative |
| SR016 | Missing closing triple-backtick on code block |

## Info

| Rule | Check |
| --- | --- |
| SR020 | Fillers: very, just, actually, really, basically, note that |
| SR021 | CAPS emphasis instead of **bold** |
| SR022 | Paragraph >4 sentences — split |
| SR023 | Bare URL instead of `[text](url)` |
| SR024 | Empty section (heading immediately followed by another heading) |
| SR025 | Trailing whitespace or multiple consecutive blank lines |

## Quality Dimensions

| Dimension | Good | Bad |
| --- | --- | --- |
| **Actionable** | Commands, checklists, yes/no rules | Essays, theory, background |
| **Scannable** | Tables, short paragraphs, bullet lists | Walls of text, nested paragraphs |
| **Complete** | Covers the skill's scope end-to-end | Gaps in workflow or missing sub-topics |
| **Focused** | Stays on the skill's domain | Drifts into other skills' territory |
| **Concrete** | Examples, templates, exact flags | Abstract principles without application |

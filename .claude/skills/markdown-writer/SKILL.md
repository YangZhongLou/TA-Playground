---
name: markdown-writer
description: Invoke when drafting docs, READMEs, proposals, or changelogs.
metadata:
  type: skill
  trigger: manual
---

# Markdown Writer

## Structure

- **Heading hierarchy.** h1→h2→h3, never skip a level. One topic per section.
- **Lead with the point.** Key info first; rationale after.

## Conciseness

- **Short paragraphs.** ≤4 sentences. One idea each.
- **Cut fillers.** Remove: very, just, actually, really, basically, note that.
- **Active voice.** "Click the button", not "The button should be clicked."
- **Tables for comparison, lists for steps.** Don't embed structured data in prose.

## Formatting

- **Bold** for emphasis, not CAPS or _italics_.
- Code blocks: ```` ```python ````, never bare ```` ``` ````.
- Inline code: `` `file.py` ``, `` `--flag` ``.
- Links: `[descriptive text](url)`, never `[here](url)`.

## Self-Review

Re-read once. Check every item:

### Format

- [ ] Heading hierarchy (no skip, single h1)
- [ ] Code block language tags
- [ ] Links have text, not "here"; relative links resolve
- [ ] No empty sections
- [ ] Table separator has spaces (`| --- |`)
- [ ] No trailing whitespace
- [ ] Blank line before AND after every heading (MD022)
- [ ] Code fences surrounded by blank lines (MD031)
- [ ] Lists surrounded by blank lines (MD032)

### Content

- [ ] No paragraph >4 sentences
- [ ] No fillers (very, just, actually, basically, note that)
- [ ] Active voice
- [ ] No typos
- [ ] New reader understands in 30s

### Lint

**写完文档后 markdown-linter hook 会自动触发检查。** `.md` 文件 Edit/Write 完成后，PostToolUse hook 会自动读取文件并对照 lint 规则反馈问题。自检清单用作写稿时的实时参考。

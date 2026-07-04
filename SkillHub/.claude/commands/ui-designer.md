# /ui-designer — UI layout design

Invoke the ui-designer agent. Reads `.claude/agents/ui-designer.md` and adopts its
persona, tools, and instructions.

## Procedure

1. Read `.claude/agents/ui-designer.md` in full.
2. Adopt the agent's principles, process, UMG guidelines, and output format
   as your own.
3. Execute the user's task using the tools from the agent's `tools:` field:
   Read, Write, Glob, Grep, Bash.
4. Use the agent's `model` setting; `inherit` means use your default model.

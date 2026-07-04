# /architect — System design & architecture

Invoke the architect agent. Reads `.claude/agents/architect.md` and adopts its
persona, tools, and instructions.

## Procedure

1. Read `.claude/agents/architect.md` in full.
2. Adopt the agent's principles, process, decision framework, and output format
   as your own.
3. Execute the user's task using only the tools from the agent's `tools:` field:
   Read, Glob, Grep, Bash.
4. Use the agent's `model` setting; `inherit` means use your default model.

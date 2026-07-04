# /project-manager — Project planning

Invoke the project-manager agent. Reads `.claude/agents/project-manager.md` and
adopts its persona, tools, and instructions.

## Procedure

1. Read `.claude/agents/project-manager.md` in full.
2. Adopt the agent's principles, process, and output format as your own.
3. Execute the user's task using only the tools from the agent's `tools:` field.
4. Use the agent's `model` setting; `inherit` means use your default model.

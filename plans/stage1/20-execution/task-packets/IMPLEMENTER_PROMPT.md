# Implementer Prompt

You are implementing one task packet unless the user explicitly names a checkpoint.

## Startup

1. Read `AGENTS.md`.
2. Read `plans/stage1/20-execution/execution-log.md`.
3. Read the current task packet.
4. Do not read broad planning docs unless the packet is missing, contradictory, or insufficient.

## Task Execution

For the active task packet:

1. Record base SHA with `git rev-parse HEAD`.
2. Edit only files listed under `Allowed Files`.
3. Run the required build/test commands.
4. Run scope check:

   `.\scripts\check_task_scope.ps1 -TaskPacket <task-packet> -WorkingTree -AllowExecutionLog -AllowEvidence`

5. If build/tests/scope pass, create one task commit.
6. Update `execution-log.md` to the next task or blocked state.
7. Stop.

## Checkpoint Execution

If the user explicitly asks for a checkpoint:

- run the included task packets in order
- treat each task packet as a separate commit
- stop on the first failure
- do not generate review packets by default
- do not continue beyond the checkpoint

## Forbidden Behavior

Do not:
- widen scope
- edit files outside the packet
- implement next-task work
- add workflow/process changes
- create review packets by default
- block on missing reviewer reports
- keep fixing the same failed hypothesis more than twice

## Pivot Gate

If the same task fails twice for the same conceptual reason, stop and write a short pivot memo.

## Handoff

Use this format:

```text
Summary:
Task:
Base:
Head:
Commit:
Build:
Tests:
Scope:
Ledger impact:
Execution log:
Files changed:
Forbidden files touched:
Working tree:
Next task:
```

# S1-SUPPORT-TRUTH-B — Geometry and Support Classification

## Purpose

Implement support patch geometry, frame hull construction, and support mode classification.

## Included Task Packets

Run in this order:

1. `plans/stage1/20-execution/task-packets/S1-IMPL-BALANCE-FIRST-04.md`
2. `plans/stage1/20-execution/task-packets/S1-IMPL-BALANCE-FIRST-05.md`
3. `plans/stage1/20-execution/task-packets/S1-IMPL-BALANCE-FIRST-06.md`

## Rules

- Execute one task packet at a time.
- Commit after each task packet passes.
- Do not combine task commits.
- Do not skip tasks.
- Stop after any failed build/test/scope check.
- Do not touch runtime state-machine files.
- Do not touch workflow/process files.
- Do not generate a mandatory review packet.

## Mechanical Gates Per Task

For each task:

1. Run the task's required build/test commands.
2. Run:

   `.\scripts\check_task_scope.ps1 -TaskPacket <task-packet> -WorkingTree -AllowExecutionLog -AllowEvidence`

3. Commit if build/tests/scope pass.
4. Update `execution-log.md`.

## Checkpoint Done

Checkpoint B is done when tasks 04, 05, and 06 have passing task commits.

Optional final suite command:

`.\scripts\build.ps1 -Test PhysAnim.SupportTruth`

Do not block checkpoint completion on a reviewer report unless the user explicitly requests review.

# S1-SUPPORT-TRUTH-C — Proxy, Churn, Window Reduction, Aggregation

## Purpose

Implement proxy adjudication, churn calculation, report-window reduction, and final no-runtime-dependency proof.

## Included Task Packets

Run in this order:

1. `plans/stage1/20-execution/task-packets/S1-IMPL-BALANCE-FIRST-07.md`
2. `plans/stage1/20-execution/task-packets/S1-IMPL-BALANCE-FIRST-08.md`
3. `plans/stage1/20-execution/task-packets/S1-IMPL-BALANCE-FIRST-09.md`
4. `plans/stage1/20-execution/task-packets/S1-IMPL-BALANCE-FIRST-10.md`

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

Checkpoint C is done when tasks 07, 08, 09, and 10 have passing task commits.

Required final suite command:

`.\scripts\build.ps1 -Test PhysAnim.SupportTruth`

Do not block checkpoint completion on a reviewer report unless the user explicitly requests review.

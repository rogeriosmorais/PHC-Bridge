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
- Commit after each task packet passes its required build/tests.
- Do not combine task commits.
- Do not skip a task.
- Do not continue after a failed build/test.
- Do not continue after a scope violation.
- Do not continue after forbidden files are touched.
- Do not start task 05 unless task 04 passes.
- Do not start task 06 unless task 05 passes.

## Checkpoint Review

After task 06 passes and is committed:

Generate a checkpoint review packet covering the full range from the base before task 04 to the head after task 06.

Reviewer must review:
- all three task packets
- all commits in the checkpoint
- changed files
- build/test evidence
- scope compliance
- no fake geometry implementation
- no runtime dependencies

## Definition Of Done

- task 04 committed
- task 05 committed
- task 06 committed
- all required builds/tests passed
- checkpoint review packet generated
- agent stops before task 07

# S1-SUPPORT-TRUTH-A — Scaffold, Harness, Value Types

## Purpose

Create the pure support module scaffold, register the automation harness, and add Slice 1 value types.

## Included Task Packets

Run in this order:

1. `plans/stage1/20-execution/task-packets/S1-IMPL-BALANCE-FIRST-01.md`
2. `plans/stage1/20-execution/task-packets/S1-IMPL-BALANCE-FIRST-02.md`
3. `plans/stage1/20-execution/task-packets/S1-IMPL-BALANCE-FIRST-03.md`

## Rules

- Execute one task packet at a time.
- Commit after each task packet passes its required build/tests.
- Do not combine task commits.
- Do not skip a task.
- Do not continue after a failed build/test.
- Do not continue after a scope violation.
- Do not continue after forbidden files are touched.
- Do not start task 02 unless task 01 passes.
- Do not start task 03 unless task 02 passes.

## Resume Rule

When executing this checkpoint, resume from `Current Task ID` in `execution-log.md`.

Do not rerun already committed task packets listed in `Completed Task Commits`.

For the current repository state:
- task 01 is already committed
- resume at task 02

## Blocked Task Handling

If a task fails after useful allowed-file edits:

- do not continue to the next task
- do not leave the working tree dirty
- create a blocker report under `plans/stage1/30-evidence/blockers/`
- update `execution-log.md` to blocked
- create a blocked-task commit
- stop

The next agent resumes from the blocked commit.

## Execution Mode

This checkpoint is the active review unit.

Do not review task 01, task 02, or task 03 separately unless a task fails.

For each included task:
- run the task packet
- run required build/tests
- commit atomically
- continue to the next task if successful

After task 03:
- generate one checkpoint review packet from checkpoint base to checkpoint head
- stop

## Checkpoint Review

After task 03 passes and is committed:

Generate a checkpoint review packet covering the full range from the base before task 01 to the head after task 03.

Reviewer must review:
- all three task packets
- all commits in the checkpoint
- changed files
- build/test evidence
- scope compliance

## Definition Of Done

- task 01 committed
- task 02 committed
- task 03 committed
- all required builds/tests passed
- checkpoint review packet generated
- agent stops before task 04

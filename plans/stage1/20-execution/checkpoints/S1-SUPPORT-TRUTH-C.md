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
- Commit after each task packet passes its required build/tests.
- Do not combine task commits.
- Do not skip a task.
- Do not continue after a failed build/test.
- Do not continue after a scope violation.
- Do not continue after forbidden files are touched.
- Do not start task 08 unless task 07 passes.
- Do not start task 09 unless task 08 passes.
- Do not start task 10 unless task 09 passes.

## Mechanical Gates

After each task packet:
- write durable build/test output under `plans/stage1/30-evidence/build/`
- run scope check for the current task packet
- commit only after build/tests and scope check pass

At checkpoint end:
- run checkpoint-wide scope check
- write checkpoint scope output to `plans/stage1/30-evidence/build/S1-SUPPORT-TRUTH-C-scope.log`
- generate checkpoint review packet with this checkpoint packet plus all included task packets
- stop after task 10

Required checkpoint scope command:

`.\scripts\check_task_scope.ps1 -CheckpointPacket plans/stage1/20-execution/checkpoints/S1-SUPPORT-TRUTH-C.md -BaseRef <checkpoint-base> -HeadRef <checkpoint-head> -AllowExecutionLog -AllowEvidence`

Required checkpoint review packet command:

`.\scripts\make_review_packet.ps1 -CheckpointPacket plans/stage1/20-execution/checkpoints/S1-SUPPORT-TRUTH-C.md -ExtraPackets plans/stage1/20-execution/task-packets/S1-IMPL-BALANCE-FIRST-07.md,plans/stage1/20-execution/task-packets/S1-IMPL-BALANCE-FIRST-08.md,plans/stage1/20-execution/task-packets/S1-IMPL-BALANCE-FIRST-09.md,plans/stage1/20-execution/task-packets/S1-IMPL-BALANCE-FIRST-10.md -BaseRef <checkpoint-base> -HeadRef <checkpoint-head> -ScopeLog plans/stage1/30-evidence/build/S1-SUPPORT-TRUTH-C-scope.log -BuildLog <build-log> -TestLog <test-log-if-any> -OutputPath plans/stage1/30-evidence/reviews/S1-SUPPORT-TRUTH-C-review-packet.md`

## Checkpoint Review

After task 10 passes and is committed:

Generate a checkpoint review packet covering the full range from the base before task 07 to the head after task 10.

Reviewer must review:
- all four task packets
- all commits in the checkpoint
- changed files
- build/test evidence
- scope compliance
- no runtime dependencies
- all Slice 1 pure support behavior covered

## Definition Of Done

- task 07 committed
- task 08 committed
- task 09 committed
- task 10 committed
- all required builds/tests passed
- checkpoint review packet generated
- agent stops

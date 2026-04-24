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

## Mechanical Gates

After each task packet:
- write durable build/test output under `plans/stage1/30-evidence/build/`
- run scope check for the current task packet
- commit only after build/tests and scope check pass

At checkpoint end:
- run checkpoint-wide scope check
- write checkpoint scope output to `plans/stage1/30-evidence/build/S1-SUPPORT-TRUTH-B-scope.log`
- generate checkpoint review packet with this checkpoint packet plus all included task packets
- stop before task 07

Required checkpoint scope command:

`.\scripts\check_task_scope.ps1 -CheckpointPacket plans/stage1/20-execution/checkpoints/S1-SUPPORT-TRUTH-B.md -BaseRef <checkpoint-base> -HeadRef <checkpoint-head> -AllowExecutionLog -AllowEvidence`

Required checkpoint review packet command:

`.\scripts\make_review_packet.ps1 -CheckpointPacket plans/stage1/20-execution/checkpoints/S1-SUPPORT-TRUTH-B.md -ExtraPackets plans/stage1/20-execution/task-packets/S1-IMPL-BALANCE-FIRST-04.md,plans/stage1/20-execution/task-packets/S1-IMPL-BALANCE-FIRST-05.md,plans/stage1/20-execution/task-packets/S1-IMPL-BALANCE-FIRST-06.md -BaseRef <checkpoint-base> -HeadRef <checkpoint-head> -ScopeLog plans/stage1/30-evidence/build/S1-SUPPORT-TRUTH-B-scope.log -BuildLog <build-log> -TestLog <test-log-if-any> -OutputPath plans/stage1/30-evidence/reviews/S1-SUPPORT-TRUTH-B-review-packet.md`

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

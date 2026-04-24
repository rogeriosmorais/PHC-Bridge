# Task Packets

This folder contains atomic implementation task packets for Stage 1.

## Operating Model

- task packet = atomic implementation unit
- one task packet = one successful commit
- checkpoint = optional batch of task packets
- mechanical proof = build/test/scope output
- execution-log = current pointer

## Task Packet Rule

Agents must not implement from broad plans directly.

Each task packet must define:
- purpose
- allowed files
- forbidden files
- required work
- required tests/build
- definition of done
- stop conditions

Agents may edit only files allowed by the active task packet.

If a task requires a file not listed in the packet, stop and report:

`Blocked: task packet does not allow required file <path>`

## Default Flow

1. Read `AGENTS.md`.
2. Read `plans/stage1/20-execution/execution-log.md`.
3. Read the current task packet.
4. Edit only allowed files.
5. Run required build/tests.
6. Run scope check.
7. Commit.
8. Update execution log.
9. Stop.

## Mechanical Gates

Before a successful task commit:

```powershell
.\scripts\check_task_scope.ps1 -TaskPacket <task-packet> -WorkingTree -AllowExecutionLog -AllowEvidence
```

Build/test commands come from the task packet.

## Checkpoints

Checkpoints may group task packets, but they are not mandatory review gates.

When executing a checkpoint:
- run task packets in order
- commit after each task
- stop on failure
- do not combine commits
- do not generate review packets by default

## Review

Review is optional unless explicitly requested.

A review should inspect:
- the task packet
- changed files
- build/test result
- scope result

Do not require durable review reports by default.

## Workflow / Product Separation

Implementation tasks must not include workflow/process changes.

Workflow/process changes must be separate commits.

# Task Packets

This folder contains legacy atomic implementation task packets for Stage 1.

These packets are historical/reference material now. Live task state, next action, dependencies, blockers, acceptance criteria, and completion status are owned by mcp-graph.

Do not execute a task packet directly unless a current mcp-graph node explicitly reactivates it or cites it as evidence.

## Operating Model

- mcp-graph node = live implementation unit
- task packet = legacy/reference implementation packet
- legacy expectation: one task packet = one successful commit
- checkpoint = legacy optional batch of task packets
- mechanical proof = historical build/test/scope output
- mcp-graph = current pointer

## Task Packet Rule

Agents must not implement from broad plans directly.

Agents also must not implement from this folder directly unless mcp-graph points here.

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
2. Pull the current mcp-graph node.
3. Read the task packet only if the graph node cites it.
4. Follow the graph node AC and active contract docs.
5. Run required build/tests.
6. Update mcp-graph status/rationale.
7. Stop.

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

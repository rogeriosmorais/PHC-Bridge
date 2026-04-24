# Task Packets

This folder contains the implementation task packets for Stage 1.

Agents must not implement from broad plans directly.

## Implementer Prompt

Implementation agents should be launched with:

`plans/stage1/20-execution/task-packets/IMPLEMENTER_PROMPT.md`

The user may simply say:

`execute current task`

or:

`go`

The agent must then read `execution-log.md`, find the current task packet, and execute only that packet.

For implementation work, read only:

1. `AGENTS.md`
2. the current task packet
3. directly edited files
4. directly relevant tests
5. build/test output

The task packet is the working context.
The refactor plan is the authority.
The execution log is the state board.

## Rules

- Do not edit files outside the current task packet.
- Do not advance to the next packet until the current packet is complete.
- Do not combine packets.
- Do not add behavior in scaffold-only packets.
- Do not add tests in packets that forbid tests.
- Do not touch runtime state-machine files unless the packet explicitly allows them.
- Do not reopen architecture unless the packet is impossible.

## Required Handoff

Every implementer handoff must end with:

`Summary: <one sentence>`
`Task: <task id>`
`Task base: <sha|none>`
`Task head: <sha|none>`
`Commit: <sha|none>`
`Review: pending|not started|review report attached`
`Ledger impact: none|updated: A-XX|blocked: assumption decision needed`
`Execution| Blocking Reason | `none` |
| Scope Check | `missing` |
| Scope Log | `none` |
| Build Log | `none` |
| Test Log | `none` |
| Working Tree | `clean at handoff` |
`Files changed: <comma-separated paths>`
`Forbidden files touched: none|<paths>`
`Next task: <task id|blocked|none>`

## Complete Task Lifecycle

Use this lifecycle for every implementation task:

1. User says `go`.
2. Implementer reads:
   - `AGENTS.md`
   - `agent_workflow_protocol.md`
   - `execution-log.md`
   - `IMPLEMENTER_PROMPT.md`
   - current task packet
3. Implementer records task base with `git rev-parse HEAD`.
4. Implementer edits only allowed files.
5. Implementer runs required build/tests.
6. If build/tests fail:
   - no commit
   - handoff reports failure
   - task remains current
7. If build/tests pass:
   - one task commit is created
   - handoff includes task base, task head, and commit SHA
8. Review packet is generated from task base to task head.
9. Reviewer uses only:
   - `REVIEWER_PROMPT.md`
   - generated review packet
10. If reviewer verdict is `accept`:
   - orchestrator may advance execution-log to the next task packet
11. If reviewer verdict is `fix required`:
   - same task remains current
   - fixes must stay inside the same task packet
12. If reviewer verdict is `reject`:
   - task commit is reverted
   - same task remains current

Do not start the next task until reviewer verdict is `accept`.

## Review Packet Command

The implementer must generate the review packet after a successful task commit.

The user does not run this manually.

Required command:

`.\scripts\make_review_packet.ps1 -TaskPacket plans/stage1/20-execution/task-packets/<TASK-ID>.md -BaseRef <task-base> -HeadRef <task-head> -BuildLog <path-if-known> -TestLog <path-if-known> -OutputPath plans/stage1/30-evidence/reviews/<TASK-ID>-review-packet.md`

The reviewer consumes the generated review packet.

The reviewer must not generate the review packet.

## Reviewer Trigger

The implementer must not self-approve.

After a task packet is implemented and committed, a review packet must be generated from task base to task head.

The reviewer receives only:
- `plans/stage1/20-execution/task-packets/REVIEWER_PROMPT.md`
- the generated review packet

The reviewer must not receive:
- broad repo context
- full conversation history
- implementer reasoning
- architecture summaries
- unrelated docs

The reviewer must not edit files.

The next task may start only after reviewer verdict:

`accept`

If verdict is `reject` or `fix required`, the next task remains blocked.

## Reviewer Prompt

Reviewer agents must be launched with:

- `plans/stage1/20-execution/task-packets/IMPLEMENTER_PROMPT.md`
- `plans/stage1/20-execution/task-packets/REVIEWER_PROMPT.md`

## Mechanical Gates

Agents must not rely on hand-written claims for scope/build/test status.

Implementation agents must create durable evidence for:
- build
- tests, if applicable
- scope check

Scope check is mandatory before every successful task commit.

Checkpoint-wide scope check is mandatory before checkpoint review.

Reviewer must reject or block any checkpoint review packet missing scope evidence.

## Clean Workflow Separation

Workflow/process changes must not be mixed into implementation checkpoints.

Allowed exceptions:
- execution-log status updates
- build/test/scope evidence files
- blocker reports
- review packets

If an implementation checkpoint requires changing AGENTS.md, workflow protocol files, scripts, task packets, or checkpoint files, stop and classify it as a workflow blocker.

Do not continue implementation until the workflow change is reviewed separately.
2. the generated review packet

The reviewer must not receive:
- broad repo context
- full conversation history
- implementer reasoning
- architecture summaries
- unrelated docs

## Command Cheatsheet

Use:

`go`

to execute the current implementation task.

Use:

`review current task`

to review the current implementation commit against the current task packet.

Use:

`fix current task`

to fix reviewer blockers inside the current task packet.

Use:

`accept current task` to advance the execution log after reviewer verdict `accept`.

## Mandatory Script Ownership

The user does not run workflow scripts manually.

Implementer agents must run:
- `.\scripts\build.ps1`
- `.\scripts\make_review_packet.ps1`

Reviewer agents must run no scripts by default.

Optional startup scripts are forbidden.

Do not create helper scripts unless they are mandatory and assigned to a role in:
- `AGENTS.md`
- `agent_workflow_protocol.md`
- this README



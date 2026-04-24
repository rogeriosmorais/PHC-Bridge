# Task Packets

This folder contains the implementation task packets for Stage 1.

Agents must not implement from broad plans directly.

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

Every task handoff must end with:

`Summary: <one sentence>`
`Ledger impact: none|updated: A-XX|blocked: assumption decision needed`
`Execution log impact: none|updated|blocked`
`Tests: <not run|passed|failed + command>`
`Build: <not run|passed|failed + command>`
`Files changed: <comma-separated paths>`
`Forbidden files touched: none|<paths>`
`Next task: <task id or none>`

## Review Packet Command

After a task commit, generate a bounded review packet with:

`.\scripts\make_review_packet.ps1 -TaskPacket plans/stage1/20-execution/task-packets/<TASK-ID>.md`

Paste only that review packet into the reviewer.

Do not ask for broad repo review after implementation commits.

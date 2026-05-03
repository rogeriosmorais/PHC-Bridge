# Decision Locks

This file records settled decisions that agents must not reopen during normal implementation.

## Locks

- Slice 1 is value-only.
- Slice 1 must not read runtime objects.
- Slice 1 must not touch runtime state-machine files.
- No runtime rewrite starts until Slice 1 pure support logic is green.
- No broad review is allowed by default.
- Implementation work must use task packets.
- Review work must evaluate the diff against the task packet.
- Visual improvement is not progress unless artifact-backed.
- A stub is not implementation.
- A shortcut that hides uncertainty is a blocker.
- The assumption ledger is a high-signal risk register, not a task log.

## Reopening Rule

A decision lock may be reopened only if:
- a task packet is impossible to execute,
- a contract contradicts another contract,
- a required test cannot be written,
- or an implementation proves the locked decision physically impossible.

If a lock is reopened, update:
- `plans/stage1/20-execution/assumption-ledger.md`
- `plans/stage1/20-execution/execution-log.md`
- the relevant task packet

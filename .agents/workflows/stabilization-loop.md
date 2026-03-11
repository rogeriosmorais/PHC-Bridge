---
description: Autonomous Stage 1 bridge stabilization loop
---
// stabilization-loop

# Stage 1 Stabilization Loop

Use this workflow for iterative UE bridge stabilization work inside the locked Stage 1 architecture.

## Goal

Run a disciplined loop until one of these outcomes occurs:

1. `stable`
   - the bridge meets the current stability target
2. `exhausted`
   - the current local stabilization strategy no longer yields meaningful progress
3. `escalate`
   - the next move would require changing frozen architecture, scope, or core bridge contracts

## Loop Contract

Each pass must follow this order:

1. inspect the latest runtime evidence
   - runtime logs
   - automation results
   - PIE smoke results
2. verify assumptions against official UE docs
3. verify semantics against the local UE engine source
4. write one narrow hypothesis for the current dominant failure
5. define a pass-local success criterion before coding
6. make one controlled code change
7. build and run:
   - component automation
   - PIE smoke
8. compare the new evidence against the success criterion
9. update the relevant documentation to reflect:
   - the current dominant failure
   - the current stabilization strategy
   - any changed manual verification expectations
10. either:
   - continue with the next narrow hypothesis
   - stop as `stable`
   - stop as `exhausted`
   - stop as `escalate`

## Rules

- Keep the Stage 1 architecture locked.
- Change one behavioral variable per pass when possible.
- Do not run multiple peer coding agents against the same bridge code at once.
- Use subagents only for bounded read-only work:
  - doc/source lookup
  - log summarization
  - diff review
- Every pass must be evidence-driven, not intuition-driven.
- Documentation updates are part of the loop, not a separate cleanup task.
- If a pass changes runtime behavior, logs, verification steps, or the active strategy, update the affected docs before concluding the pass.

## Documentation Outputs

The loop owns these documentation surfaces:

- [STAGE1_PLAN.md](/F:/NewEngine/STAGE1_PLAN.md)
  - update when the active Stage 1 stabilization direction or execution status changes
- [manual-verification.md](/F:/NewEngine/plans/stage1/60-user/manual-verification.md)
  - update when operator steps, expected logs, or pass/fail expectations change
- [phase1-implementation-package.md](/F:/NewEngine/plans/stage1/20-execution/phase1-implementation-package.md)
  - update when the active bridge implementation strategy changes materially
- [ARCHITECTURE_REVIEW_RESPONSE.md](/F:/NewEngine/ARCHITECTURE_REVIEW_RESPONSE.md)
  - update only when the stabilization results materially change the standing architectural argument

Do not rewrite all planning docs every pass. Update only the documents whose contract changed.

## Default Verification Commands

Build:
```powershell
& "E:\UE_5.7\Engine\Build\BatchFiles\Build.bat" PhysAnimUE5Editor Win64 Development -Project="F:\NewEngine\PhysAnimUE5\PhysAnimUE5.uproject" -Progress -NoHotReloadFromIDE
```

Component automation:
```powershell
& "E:\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "F:\NewEngine\PhysAnimUE5\PhysAnimUE5.uproject" -ExecCmds="Automation RunTests PhysAnim.Component; Quit" -unattended -nop4 -nosplash -NullRHI -log
```

PIE smoke:
```powershell
& "F:\NewEngine\scripts\run-pie-smoke.ps1"
```

## Stop Policy

Continue while each pass does at least one of:

- narrows the fault surface
- disproves a plausible hypothesis
- improves stability duration or severity
- improves observability enough to justify the next pass

Stop as `exhausted` when:

- 2 to 3 consecutive disciplined passes do not improve the same isolated fault, and
- the next move would no longer be a local stabilization change

Stop as `escalate` when:

- the next move would require changing:
  - bridge contract
  - mapping strategy
  - model contract
  - architecture lock

## Current Stage 1 Target

Current stabilization target:

- the one-character bridge survives staged activation in PIE smoke
- no catastrophic per-body spikes appear during the required runtime window
- logs remain readable enough that later tuning or G2 work is meaningful

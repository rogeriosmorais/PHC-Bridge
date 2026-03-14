---
name: Runtime Stabilization
description: Controlled stabilization loop for the locked Stage 1 runtime bridge
---

# Runtime Stabilization

Use this for iterative runtime stabilization inside the locked Stage 1 architecture.

## Goal

Stop only when one of these is true:
- stable
- exhausted
- escalate

## Loop

Each pass should follow this order:

1. inspect the latest evidence
   - logs
   - automation results
   - PIE smoke results

2. define the dominant current failure

3. verify assumptions if needed
   - UE docs
   - local engine source
   - existing bridge code

4. write one narrow hypothesis

5. define one pass-local success criterion

6. make one controlled change

7. run the smallest relevant verification
   - component automation
   - PIE smoke
   - full build only when needed

8. compare evidence against the success criterion

9. update only the docs whose contract actually changed

## Rules

- keep the Stage 1 architecture locked
- change one behavioral variable per pass when possible
- prefer evidence over intuition
- do not run multiple peer coding agents on the same runtime bridge code at once
- use subagents only for bounded read-only support work

## Default Verification

Build:
```powershell
& "$UE5_PATH\Build\BatchFiles\Build.bat" PhysAnimUE5Editor Win64 Development `
  -Project="PhysAnimUE5\PhysAnimUE5.uproject" `
  -Progress -NoHotReloadFromIDE
```

Component automation:
```powershell
& "$UE5_PATH\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "PhysAnimUE5\PhysAnimUE5.uproject" `
  -ExecCmds="Automation RunTests PhysAnim.Component; Quit" `
  -unattended -nop4 -nosplash -NullRHI -log
```

PIE smoke:
```powershell
powershell -ExecutionPolicy Bypass -File scripts/run-pie-smoke.ps1
```

## Stop Policy

Continue while each pass does at least one of:
- narrows the fault surface
- disproves a plausible hypothesis
- improves runtime stability
- improves observability enough to justify another pass

Stop as exhausted when:
- multiple disciplined passes fail against the same isolated fault
- the next move is no longer a local stabilization change

Stop as escalate when:
- the next move would require changing a frozen bridge contract
- the next move would require changing the model contract
- the next move would require changing the architecture lock

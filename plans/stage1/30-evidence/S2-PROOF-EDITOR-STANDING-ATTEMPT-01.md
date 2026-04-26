# Evidence: S2-PROOF-EDITOR-STANDING-ATTEMPT-01 (Standing Proof)

## Status
- **Mechanical Preflight**: PASSED
- **Evidence Capture Prep**: PASSED
- **Editor PIE Proof**: PENDING USER EXECUTION

## PIE Execution Checklist (Operator: USER)
- [ ] Open UE5 Editor.
- [ ] Select Actor with `UPhysAnimComponent`.
- [ ] Enable `bEnableLiveRuntimeEvidenceProof` in Details.
- [ ] Start PIE (Play In Editor).
- [ ] Observe Character Attempt to stand.
- [ ] Allow 3-5 seconds of simulation.
- [ ] Stop simulation.
- [ ] Open `Window -> Output Log`.
- [ ] Right-click -> `Select All` -> `Copy`.
- [ ] Save log to `f:\NewEngine\PhysAnimUE5\Saved\Logs\StandingProof.log` (or similar).

## Log Extraction Patterns
Search for these patterns in the Output Log:
- `PhysAnimProof: AttemptStart` -> Verified start of sequence.
- `PhysAnimProof: StandingProgress` -> Per-tick telemetry.
- `PhysAnimProof: TerminalArtifact` -> Failure reason and snapshot (Expected only on FAIL).
- `PhysAnimProof: AttemptResult` -> Final verdict (PASS/FAIL).

### Extraction Command
Run this in PowerShell after saving the log:
```powershell
Select-String -Path "PhysAnimUE5/Saved/Logs/StandingProof.log" -Pattern "PhysAnimProof:"
```

## Validation Criteria
- **PASS**: `AttemptResult verdict=PASS` and `duration >= 3.0`.
- **FAIL**: `AttemptResult verdict=FAIL`, `duration < 3.0`, and exactly ONE `TerminalArtifact` entry.

## Telemetry (PENDING)
### standing_duration_seconds
- **Target**: 3.0s
- **Actual**: [TBD]

### Terminal Reason
- **Actual**: [TBD]

### Log Snippet
```text
[TBD: Paste extracted PhysAnimProof: logs here]
```

## Visual Evidence
- **Screenshot/Clip**: [TBD: Confirm capture]

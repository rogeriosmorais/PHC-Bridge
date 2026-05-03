# Task Packet: S2-PROOF-EDITOR-STANDING-ATTEMPT-01

## Purpose
Execute a live Editor-runtime standing benchmark using the `PhysAnimProof` hook to verify the deterministic governance pipeline in a real simulation environment.

## Allowed Files
- `plans/stage1/20-execution/execution-log.md`
- `plans/stage1/30-evidence/S2-PROOF-EDITOR-STANDING-ATTEMPT-01.md`

## Forbidden Files
- All runtime C++ files (Tuning/Architecture freeze)
- All test files

## Required Work
1. **Enable Proof Hook**: In the UE5 Editor, select the character with `UPhysAnimComponent` and enable `bEnableLiveRuntimeEvidenceProof`.
2. **Standing Attempt**:
   - Run the simulation (PIE).
   - Ensure the character attempts to stand.
   - Monitor `LogPhysAnimBridge` for `PhysAnimProof:` entries.
3. **Capture Evidence**:
   - Record the terminal log output for at least one attempt.
   - Capture a side-by-side clip (or screenshots) of the Kinematic/Reference pose vs. the Physics-driven pose if possible.
4. **Validation**:
   - Verify `standing_duration_seconds` reaches 3.0s (PASS) or fails with a truthful `terminal_reason`.
   - Ensure only one terminal artifact is emitted on FAIL.

## Required Tests/Build
- Build must be clean (no changes to C++ expected).
- Smoke tests `PhysAnim.RuntimeTermination` must pass.

## Definition of Done
- One standing attempt recorded with full `PhysAnimProof` telemetry.
- Evidence file created/updated under `plans/stage1/30-evidence/`.
- Handoff report generated with "PASS" or "FAIL" status.

## Stop Conditions
- Crash in `TickLiveRuntimeEvidenceProof`.
- No logs appearing despite `bEnableLiveRuntimeEvidenceProof` being true.
- Multiple terminal artifacts emitted for a single failure.

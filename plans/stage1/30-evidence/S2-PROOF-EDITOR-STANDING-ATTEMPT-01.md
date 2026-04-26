# Live Standing Proof Evidence (Attempt 01)

## Meta
- **Date**: 2026-04-26
- **Method**: Automated PIE Test (`PhysAnim.StandingProof.Live`)
- **Map**: `/Game/ThirdPerson/Lvl_ThirdPerson`
- **Component**: `UPhysAnimComponent` on `BP_PhysAnimCharacter`
- **Attempt UUID**: `BCAEE865-4C0A-78A5-2087-19ACB4A0AD62`

## Verdict
**FAIL**

## Reason
`ActivationSupportFailure` (Terminal Reason Index 5)

## Summary
The character was spawned in the air or fell immediately. 
The support logic detected `Airborne` mode and the `SupportGapTimerMs` quickly accumulated 495ms, triggering termination.

## Artifact (JSON Excerpt)
```json
{
	"attempt_uuid": "BCAEE865-4C0A-78A5-2087-19ACB4A0AD62",
	"standing_seconds_at_emit": 0.49560511112213135,
	"support_mode_name": "Airborne",
	"support_gap_timer_ms": 495.60509490966797,
	"active_support_side_count": 0,
	"terminal_reason_name": "ActivationSupportFailure",
	"terminal_substep_timestamp": 2,
	"terminal_frame_artifact_captured": true
}
```

## Logs (Filtered)
```text
[2026.04.26-20.54.14:722][238]LogPhysAnimBridge: Error: PhysAnimProof: TerminalArtifact uuid=BCAEE865-4C0A-78A5-2087-19ACB4A0AD62 terminal_reason=ActivationSupportFailure timestamp=2 support_mode=Airborne active_sides=0 hull_area=0.000 support_gap=495.605 proxy_inside=unset proxy_outside_duration=unset terminal_frame_captured=1 coterminal_count=0 artifact_json=F:\NewEngine\PhysAnimUE5\Saved\PhysAnim\ProofArtifacts\BCAEE865-4C0A-78A5-2087-19ACB4A0AD62_terminal.json artifact_json_written=1
[2026.04.26-20.54.14:722][238]LogPhysAnimBridge: Error: PhysAnimProof: AttemptResult uuid=BCAEE865-4C0A-78A5-2087-19ACB4A0AD62 verdict=FAIL duration=0.496 terminal_reason=ActivationSupportFailure
```

## Interpretation
The automated proof pipeline is working. It successfully:
1. Spawned the character.
2. Hooked into the component.
3. Captured telemetry.
4. Evaluated termination logic.
5. Emitted a machine-readable JSON artifact.
6. Logged the final verdict.

The failure is expected as the character is currently in a "BalanceSafeDeny" state (from logs) or simply lacks the motor control to stay upright in this version of the bridge.

# Balance Mode Smoke Evidence - 2026-05-07

## Scope

Graph task: `S1-P1-U1`

Command:

```text
.\scripts\build.ps1 -Test PhysAnim.PIE.BalanceModeSmoke
```

Log read command:

```text
python .\scripts\read_logs.py
```

Current-repo log:

```text
F:\NewEngine-AgentB\PhysAnimUE5\Saved\Logs\PhysAnimUE5.log
```

## Initial Result

The initial Unreal automation test process exited successfully.

The log shows:

- `Automation RunTests PhysAnim.PIE.BalanceModeSmoke`
- `Test Started. Name={BalanceModeSmoke} Path={PhysAnim.PIE.BalanceModeSmoke}`
- repeated `METRICS_TICK state=BalanceActive_Standing`
- standing-state timestamps beyond `12.0` seconds
- `EndEvents: PhysAnim.PIE.BalanceModeSmoke`

## Blocking Truth Issue

The same `BalanceActive_Standing` metrics lines report:

```text
PelvisSim=0 PelvisAwake=0
```

That conflicts with the active truth model, which requires physical continuity for the truth set before a balance proof can be accepted as live physics.

## Mitigation Result

Graph task: `S2-FIX-BALANCE-SMOKE-PHYSICAL-CONTINUITY-TRUTH-01`

After the mitigation, the same command exits successfully as a diagnostic smoke. It no longer reports the zero-simulation condition as a standing balance pass.

Latest artifact evidence:

```text
artifact_json=F:/NewEngine-AgentB/PhysAnimUE5/Saved/PhysAnim/ProofArtifacts/1D3AA1AB-4059-B697-E981-1EA341BAB3D4_terminal.json
terminal_reason_name=ActivationContinuousSimulationLost
standing_seconds_at_emit=3.0000343322753906
runtime_simulating_body_count=0
physical_continuity_validator_passed=false
terminal_frame_artifact_captured=true
```

Latest log evidence:

```text
PhysAnimProof: AttemptResult uuid=1D3AA1AB-4059-B697-E981-1EA341BAB3D4 verdict=FAIL duration=3.000 terminal_reason=ActivationContinuousSimulationLost
[PhysAnim] Startup proof rejected without physical continuity evidence state=WaitingForPoseSearch simBodies=0 continuity=0
EndEvents: PhysAnim.PIE.BalanceModeSmoke
```

## Decision

This run is useful progress for the state-machine and smoke-test path, and the earlier false standing pass is now blocked by the smoke outcome and proof artifact.

It is not sufficient to approve G2 or declare true balance mode complete. The current checkout now reports the missing physical continuity as the canonical terminal failure `ActivationContinuousSimulationLost`.

## Startup Physical-Continuity Classifier Result

Graph task: `S2-FIX-BALANCE-STARTUP-PHYSICAL-CONTINUITY-01`

After adding a deterministic startup proof contract for physical-continuity evidence, the same PIE smoke remains a diagnostic pass, but the blocker is now classified more precisely:

```text
artifact_json=F:/NewEngine-AgentB/PhysAnimUE5/Saved/PhysAnim/ProofArtifacts/13114163-4C6C-EE1C-6768-D093D1594F06_terminal.json
terminal_reason_name=ActivationPhysicsNotStarted
standing_seconds_at_emit=3.0000369548797607
runtime_simulating_body_count=0
physical_continuity_validator_passed=false
support_mode_name=TwoFootStable
active_support_side_count=2
support_hull_area_cm2=2090.5990655224305
```

Latest log evidence:

```text
PhysAnimProof: AttemptResult uuid=13114163-4C6C-EE1C-6768-D093D1594F06 verdict=FAIL duration=3.000 terminal_reason=ActivationPhysicsNotStarted
[PhysAnim] Startup proof rejected without physical continuity evidence state=WaitingForPoseSearch simBodies=0 continuity=0
EndEvents: PhysAnim.PIE.BalanceModeSmoke
```

This narrows the next blocker: support evidence is present, but the startup proof is still completing while the runtime is in `WaitingForPoseSearch`, before bridge physics ownership creates simulated truth-set bodies.

## Startup Physics-Ownership Sequencing Result

Graph task: `S2-FIX-BALANCE-STARTUP-PHYSICS-OWNERSHIP-SEQUENCING-01`

After adding the sequencing contract, valid PoseSearch can start bridge physics ownership before the startup proof publishes `BalanceActive_Standing`. The same PIE smoke remains a diagnostic pass and no longer terminates with `ActivationPhysicsNotStarted`.

Latest log evidence:

```text
[PhysAnimBalance] WIRING_PRE_PROOF_PHYSICS_OWNERSHIP state=BridgeActive
Snapshot[AfterActivateBridgePhysicsState] state=BridgeActive bridgeOwnsPhysics=true skeletalSim=1 rootBodySim=true
Snapshot[AfterActivationPrepass] state=BridgeActive bridgeOwnsPhysics=true skeletalSim=0 rootBodySim=false
PhysAnimProof: AttemptResult uuid=7B595134-4584-BA9B-D9C5-919A01887525 verdict=FAIL duration=3.000 terminal_reason=ActivationContinuousSimulationLost
[PhysAnim] Startup proof rejected without physical continuity evidence state=BridgeActive simBodies=0 continuity=0
EndEvents: PhysAnim.PIE.BalanceModeSmoke
```

Latest artifact evidence:

```text
artifact_json=F:/NewEngine-AgentB/PhysAnimUE5/Saved/PhysAnim/ProofArtifacts/7B595134-4584-BA9B-D9C5-919A01887525_terminal.json
terminal_reason_name=ActivationContinuousSimulationLost
standing_seconds_at_emit=3.000077486038208
runtime_simulating_body_count=0
physical_continuity_validator_passed=false
support_mode_name=TwoFootStable
active_support_side_count=2
runtime_body_sample_count=7898
```

This removes the pre-ownership blocker and exposes the next concrete blocker: activation starts skeletal simulation, but the activation prepass/body-modifier path drops the root body back to non-simulating before proof sampling can collect physical continuity.

## Activation Prepass Raw-Simulation Preservation Result

Graph task: `S2-FIX-BALANCE-ACTIVATION-PREPASS-RAW-SIM-PRESERVATION-01`

After preserving raw simulation after Physics Control updates, the same PIE smoke remains a diagnostic pass and no longer fails with zero simulated runtime bodies.

Latest log evidence:

```text
Snapshot[AfterActivationPrepass] state=BridgeActive bridgeOwnsPhysics=true skeletalSim=1 rootBodySim=true
METRICS_TICK state=BridgeActive t=0.000 PelvisSim=1 PelvisAwake=1
METRICS_TICK state=BridgeActive t=1.300 PelvisSim=1 PelvisAwake=1
PhysAnimProof: AttemptResult uuid=DEEF2096-424D-DA31-8B13-4081D5D041FE verdict=FAIL duration=1.308 terminal_reason=ActivationProxyOutsideSupportRegion
EndEvents: PhysAnim.PIE.BalanceModeSmoke
```

Latest artifact evidence:

```text
artifact_json=F:/NewEngine-AgentB/PhysAnimUE5/Saved/PhysAnim/ProofArtifacts/DEEF2096-424D-DA31-8B13-4081D5D041FE_terminal.json
terminal_reason_name=ActivationProxyOutsideSupportRegion
standing_seconds_at_emit=1.308349609375
runtime_simulating_body_count=22
physical_continuity_validator_passed=true
runtime_max_body_linear_speed_cm_per_second=150.91788310057851
runtime_max_body_angular_speed_deg_per_second=426.4466341716647
proxy_inside_hull=false
proxy_outside_hull_duration_ms=100.00079727172852
support_mode_name=TwoFootStable
active_support_side_count=2
support_hull_area_cm2=1877.4029547173991
```

This clears the raw-simulation continuity blocker. The next blocker is now support geometry / proxy coupling: physical continuity is live, but the COM proxy leaves the support hull after about 1.3 seconds while the runtime is still in `BridgeActive`.

## Startup Support/Proxy Coupling and Entry Trigger Result

Graph task: `S2-FIX-BALANCE-STARTUP-SUPPORT-PROXY-COUPLING-01`

After switching BridgeActive startup proof proxy sampling to raw simulated COM, deferring the startup proxy terminal until the proof boundary, and enabling the BridgeActive policy influence ramp after proof completion, the same smoke advances beyond the prior proxy terminal.

Latest proof artifact evidence:

```text
artifact_json=F:/NewEngine-AgentB/PhysAnimUE5/Saved/PhysAnim/ProofArtifacts/BAE01169-4422-5A24-9C00-3996A4EE0566_terminal.json
terminal_reason_name=None
standing_seconds_at_emit=3.0000436305999756
runtime_simulating_body_count=22
physical_continuity_validator_passed=true
support_mode_name=TwoFootStable
active_support_side_count=2
support_hull_area_cm2=1334.022428410053
proxy_inside_hull=false
proxy_outside_hull_duration_ms=675.00539302825928
runtime_max_body_linear_speed_cm_per_second=1655.5827711957584
runtime_max_body_angular_speed_deg_per_second=4326.0508209007949
```

Latest log evidence:

```text
[PhysAnim] Startup proof completed; BridgeActive policy influence ramp enabled for balance entry.
PhysAnimProof: AttemptResult uuid=BAE01169-4422-5A24-9C00-3996A4EE0566 verdict=PASS duration=3.000 terminal_reason=None
[PhysAnimBalance] Request status: balance_start_queued. reason=auto_trigger
[PhysAnim] State Transition: BridgeActive -> BalanceEntry_Prepare
[PhysAnim] State Transition: BalanceEntry_Prepare -> BalanceEntry_LateValidate
[PhysAnimBalance] PHASE1_LATE_VALIDATE_NONCONVERGENCE primary=phase1_root_on_readiness_pelvis_angular_incoherent secondary=none
[PhysAnimBalance] PHASE2_SAFE_DENIED phase1_root_on_readiness_pelvis_angular_incoherent
[PhysAnimPieBalanceSmoke] BalanceSafeDeny is not a benchmark success. reason=phase1_root_on_readiness_pelvis_angular_incoherent truthful=true
```

This clears the previous BridgeActive startup proof/proxy blocker enough to enter the balance transition path. The next blocker is now Phase 1 pelvis/root-on readiness: late validation holds `readyProven=0` and denies with `phase1_root_on_readiness_pelvis_angular_incoherent`.

## Phase 1 Baseline-Compensated RootOn Readiness Result

Graph task: `S2-FIX-BALANCE-PHASE1-ROOTON-READINESS-PELVIS-ANGULAR-COHERENCE-01`

The Manny PhysicsAsset contains authored constraint-frame angular offsets on direct pelvis links. Raw angular error alone made the Phase 1 RootOn readiness contract impossible for at least the pelvis/thigh links, because the authored frame floor can be larger than the readiness threshold before runtime motion is considered.

Latest diagnostic evidence:

```text
[PhysAnimBalance] PHASE2_ENTRY from=BalanceEntry_LateValidate rootOnReady=1 shellHoldReady=1 bringUpReady=1 policyInfluenceReady=1
[PhysAnimBalance] PHASE1_PELVIS_COUPLING_SKIPPED reason=noCouplingProof state=BalanceEntry_LateValidate
[PhysAnimBalance] PHASE2_ROOT_ON_LINK_ANGULAR_FORENSIC link=pelvis_thigh_l ... angularErrorDeg=53.47 authoredAngularFloorDeg=55.83 baselineCompensatedAngularErrorDeg=0.00
[PhysAnimBalance] PHASE2_ROOT_ON_LINK_ANGULAR_FORENSIC link=pelvis_spine_01 ... angularErrorDeg=43.48 authoredAngularFloorDeg=14.46 baselineCompensatedAngularErrorDeg=29.02
[PhysAnimBalance] PHASE2_SAFE_DENIED phase2_root_on_warm_start_incoherent
[PhysAnimPieBalanceSmoke] BalanceSafeDeny is not a benchmark success. reason=phase2_root_on_warm_start_incoherent truthful=true
```

Decision:

Phase 1 RootOn readiness now uses a baseline-compensated angular metric for direct pelvis-link coherence while preserving raw angular forensic logs. The smoke no longer ends at `phase1_root_on_readiness_pelvis_angular_incoherent`; it reaches Phase 2 entry with `rootOnReady=1`.

This is not true balance completion. The next truthful blocker is Phase 2 warm-start coherence: `phase2_root_on_warm_start_incoherent`.

## Phase 2 Baseline-Compensated Warm-Start Result

Graph task: `S2-FIX-BALANCE-PHASE2-ROOTON-WARM-START-COHERENCE-01`

Phase 2 warm-start now applies the same baseline-compensated direct pelvis-link angular semantics used by Phase 1 readiness. Raw angular values remain logged with the authored angular floor and compensated angular value.

Latest diagnostic evidence:

```text
PhysAnimProof: AttemptResult uuid=257F4559-44DE-0D45-7F04-87AF9ECE67DE verdict=PASS duration=3.000 terminal_reason=None
[PhysAnimBalance] PHASE2_ENTRY ... rootOnReady=1 ...
[PhysAnimBalance] PHASE2_ROOT_ON_LINK_ERROR_PRE link=pelvis_thigh_l errorCm=4.88 threshold=8.00 angularErrorDeg=54.70 authoredAngularFloorDeg=55.83 baselineCompensatedAngularErrorDeg=0.00 angularThreshold=45.00
[PhysAnimBalance] PHASE2_ROOT_ON_LINK_ERROR_PRE link=pelvis_thigh_r errorCm=5.67 threshold=8.00 angularErrorDeg=39.28 authoredAngularFloorDeg=125.23 baselineCompensatedAngularErrorDeg=0.00 angularThreshold=45.00
[PhysAnimBalance] PHASE2_ROOT_ON_LINK_ERROR_PRE link=pelvis_spine_01 errorCm=2.09 threshold=8.00 angularErrorDeg=44.70 authoredAngularFloorDeg=14.46 baselineCompensatedAngularErrorDeg=30.24 angularThreshold=35.00
[PhysAnimBalance] PHASE3_ENTRY_AUDIT frame=1124 tick=0 rootRawSim=1 pelvisRawSim=1 pelvisModifierName=Simulated simCountPre=5 simCountPost=6 shellLocked=1 shellReanchored=1
[PhysAnimBalance] PHASE3_FIRST_FAILURE_AUDIT frame=1127 reason=phase3_material_shell_correction tick=3 rootRawSim=1 pelvisRawSim=1 pelvisModifierName=Simulated simCountPost=6 shellVelocityDelta=46.30/10.00
[PhysAnimBalance] TRANSITION_SAFE_DENIED final_outcome. reason=phase3_material_shell_correction
[PhysAnimPieBalanceSmoke] BalanceSafeDeny is not a benchmark success. reason=phase3_material_shell_correction truthful=true
```

Decision:

The smoke no longer ends at `phase2_root_on_warm_start_incoherent`; it enters Phase 3/Settle with simulated root and pelvis. This clears the Phase 2 warm-start angular blocker.

This is still not true balance completion. The next truthful blocker is Phase 3 material shell correction: `phase3_material_shell_correction`.

## Phase 3 Material Shell Correction Classification Result

Graph task: `S2-FIX-BALANCE-PHASE3-MATERIAL-SHELL-CORRECTION-01`

Phase 3 Settle now distinguishes zero-offset shell carry-through from material shell drift during the early explicit-lock handoff. Bounded early carry-through and the tick-4 RootOn snap shape no longer classify as material shell correction by shell velocity alone. The Settle instability grace also now requires observed non-root angular velocity to remain inside the RootOn carry-through envelope before suppressing an instability failure.

Latest diagnostic evidence:

```text
[PhysAnimBalance] PHASE3_ENTRY_AUDIT frame=1124 tick=0 rootRawSim=1 pelvisRawSim=1 pelvisModifierName=Simulated simCountPre=5 simCountPost=6 shellLocked=1 shellReanchored=1
[PhysAnimBalance] PHASE3_FIRST_FAILURE_AUDIT frame=1128 reason=phase3_post_root_on_instability tick=4 rootRawSim=1 pelvisRawSim=1 pelvisModifierName=Simulated simCountPost=6 shellLocked=1 shellReanchored=1 rootLinear=461.16/3000.00 rootAngular=3393.35/2160.00 shellOffsetDelta=0.00/2.00 shellVelocityDelta=1118.03/10.00 prePhase3PeakNonRootAngular=1829.62 observedNonRootAngularEnvelope=1829.62 currentMaxNonRootAngular=3092.90 currentMaxNonRootAngularBone=spine_03 observedNonRootFamilyAngularEnvelope=2455.68 currentNonRootFamilyAngular=8205.23 shellCorrectionActive=1
[PhysAnimBalance] TRANSITION_SAFE_DENIED final_outcome. reason=phase3_post_root_on_instability
[PhysAnimPieBalanceSmoke] BalanceSafeDeny is not a benchmark success. reason=phase3_post_root_on_instability truthful=true
```

Decision:

The smoke no longer ends at `phase3_material_shell_correction`. It now exposes the next truthful blocker: post-RootOn Phase 3 instability, with root angular velocity and non-root angular expansion exceeding the observed carry-through envelope.

This is still not true balance completion.

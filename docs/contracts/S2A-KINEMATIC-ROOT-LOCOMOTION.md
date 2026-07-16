# Stage 2A: Kinematic-Shell Causal Reference-Tracking Contract

## Purpose

Stage 2A proves that the PHC policy causally controls a fully simulated humanoid body while that body follows a prescribed kinematic shell trajectory. It is an integration milestone between causal standing and later simulated-root locomotion research.

Stage 2A does **not** claim self-propelled physical locomotion. The shell owns global root translation and yaw. A stronger locomotion claim requires Stage 2B or another explicitly versioned simulated-root/center-of-mass authority contract.

## 1. Authority

- Root authority remains `Stage1_KinematicRoot`.
- Motion authority is the bridge-owned `Stage2A_KinematicShell`.
- The authoritative scripted product fixture uses no human input.
- `CharacterMovementComponent` must be inactive during the measured episode.
- `Phase3_SimRoot` and any equivalent simulated-pelvis handoff are forbidden.
- Capsule, shell, or kinematic-root movement may prescribe the route, but it must not directly pose or animate the simulated body.

## 2. Runtime pipeline

The required path is:

`scripted intent -> bridge shell -> Pose Search trajectory -> selected reference animation -> canonical PHC observations -> ONNX policy -> Physics Control targets -> Chaos bodies`

The policy must remain active during acceleration, cruise, moving turn, deceleration, explicit stop, and settling. Shell motion alone is not success.

## 3. Success claim

A Stage 2A product pass supports only this statement:

> Under the locked scripted route and root authority, the conditioned policy causes the simulated humanoid to make positive route-aligned physical progress, track the kinematic shell better than causal negative controls, preserve physical validity, and return cleanly to standing.

The pass requires all of the following:

1. **Route completion** — the shell executes the locked path, turn, stop, and settle schedule.
2. **Policy continuity** — inference, action conditioning, target publication, and target readback cover the locomotion interval.
3. **Reference validity** — Pose Search publishes valid non-idle movement references during sustained motion without selecting an asset by name.
4. **Physical progress** — physical-root route-projected progress is positive and materially exceeds ZeroActions.
5. **Shell tracking** — tracking-error endpoints satisfy locked absolute and comparative gates.
6. **Physical validity** — support, penetration, tilt, body topology, and readback remain valid.
7. **Terminal state** — explicit stop returns to `BalanceActive_Standing` with zero final shell speed.
8. **Causal discrimination** — registered negative and destructive controls fail or underperform for their preregistered reasons.
9. **Determinism** — the bundle satisfies its declared byte, numerical, behavioral, and process-scope requirements.

## 4. Failure modes

The following are explicit failures:

- **Statue reward** — the shell moves while the physical body remains approximately stationary.
- **Reversed motion** — physical-root progress projects negatively onto the shell route.
- **Lateral divergence** — physical path length is high but route-projected progress is low and lateral displacement dominates.
- **CharacterMovement-only motion** — forbidden movement assistance is active.
- **Shell/body divergence** — the physical character does not remain within the locked tracking envelope.
- **Inactive policy** — actions, target writes, or readbacks are absent or frozen.
- **Hard-coded reference** — a locomotion asset is selected by name instead of through Pose Search.
- **SimRoot regression** — root authority is handed to Chaos during Stage 2A.
- **Protocol mismatch** — runtime schedule, speed authority, variants, or evidence schema differ from the declared locked protocol.
- **Selective bundle** — failed, duplicated, missing, or unexpected repetitions are omitted or substituted.

## 5. Required evidence

Every authoritative run must identify:

- source commit and dirty-tree status;
- protocol path, version, and digest;
- ONNX model digest;
- environment/authority fingerprint;
- root authority and motion source;
- exact scripted phase and intent;
- shell position, yaw, velocity, path length, and net displacement;
- physical-root position, path length, net displacement, route-projected progress, and lateral displacement;
- root-shell tracking error per sample plus AUC, average, final, and maximum;
- selected Pose Search asset and selected time;
- policy inference/action/write/readback evidence;
- support, penetration, tilt, topology, and forbidden-assistance evidence;
- final runtime state and final shell speed.

## 6. Metric structure

Protocol v2 should use separate interpretable endpoints rather than one standing-derived weighted score:

- primary causal endpoint: physical-root progress projected onto the shell route;
- comparative endpoint: Normal versus ZeroActions progress and tracking;
- tracking endpoints: error AUC, time average, final error, and maximum error;
- hard validity gates: support, penetration, tilt, readback, topology, assistance, and terminal state;
- destructive-control gates: trajectory-conditioning and stop-transition discrimination;
- exact bundle-membership and authority-fingerprint gates.

Thresholds belong only in a versioned, locked product protocol. This contract defines the concepts, not the current numerical acceptance values.

## 7. Architectural boundary

Stage 2A must remain decoupled from Stage 2B. No Stage 2A product path may depend on simulated-root handoff behavior. Stage 2B may consume Stage 2A evidence as a baseline, but it requires its own authority, perturbation, and acceptance protocol.

# Balance Perturbation Mode Design

## Objective
The Balance Perturbation Mode is a dedicated diagnostic framework designed to measure the balance recovery capabilities of the PHC-driven physics character. It isolates the neural network's corrective body dynamics from locomotion assistance, shell translation, or external movement anchors.

## 1. Perturbation Method

### Authoritative Method: Pelvis Body Impulse
- **Target**: The `pelvis` rigid body of the character's skeletal mesh.
- **Type**: Instantaneous linear impulse applied once per scenario.
- **Coordinate Space**: World-space directions (+X, -X, +Y, -Y).
- **Execution**: Directly via `FBodyInstance::AddImpulse` or `USkeletalMeshComponent::AddImpulse` targeting the pelvis bone.

### Rejected Methods & Why
- **Shell Offset / `perturbOverride`**: REJECTED. It translates the reference frame, effectively "helping" the character by moving the goalposts rather than testing the body's ability to resist force.
- **Actor/Capsule Displacement**: REJECTED. These are driven by `CharacterMovementComponent`, which contains hidden locomotion logic. We want to test pure PHC balance.
- **Upper-Body Shove**: REJECTED (as primary test). Shoving the spine/head creates joint shearing that doesn't test the primary balance loop (CoM over Support).
- **Continuous Force**: REJECTED. Harder to quantify and compare. Impulse allows for a clear "time-to-recovery" metric.

## 2. Runtime Authority & Gating

### Balance Perturbation Mode State
When active, this mode enforces a "Statue Mode" for the locomotion system:
- **Idle Reference Only**: The policy is fed a static idle pose as the mimic target.
- **Locomotion Disabled**: `CharacterMovementComponent` is essentially locked or ignored.
- **Shell Clamping**: The locomotion "shell" (the invisible anchor) is pinned in world space. Any actor movement during recovery is considered "shell contamination".

### Trigger Conditions
1. **Bridge State**: `BridgeActive`.
2. **Stable Window**: Minimum 1.0s of undisturbed standing after activation or previous recovery.
3. **Recovery Window**: No active impulse sequence is currently running.
4. **Locomotion State**: Must be idle.

## 3. Scenario Matrix

Each scenario is a deterministic trial.

| Scenario Name | Direction | Magnitude | Impulse Value (kg*cm/s) |
| :--- | :--- | :--- | :--- |
| **IdleHold_NoPush** | N/A | None | 0 |
| **IdleHold_PelvisForward_Small** | +X | Small | 20,000 |
| **IdleHold_PelvisForward_Medium** | +X | Medium | 80,000 |
| **IdleHold_PelvisForward_Large** | +X | Large | 200,000 |
| **IdleHold_PelvisBackward_Small** | -X | Small | 20,000 |
| **IdleHold_PelvisBackward_Medium** | -X | Medium | 80,000 |
| **IdleHold_PelvisBackward_Large** | -X | Large | 200,000 |
| **IdleHold_PelvisLeft_Small** | -Y | Small | 20,000 |
| **IdleHold_PelvisLeft_Medium** | -Y | Medium | 80,000 |
| **IdleHold_PelvisLeft_Large** | -Y | Large | 200,000 |
| **IdleHold_PelvisRight_Small** | +Y | Small | 20,000 |
| **IdleHold_PelvisRight_Medium** | +Y | Medium | 80,000 |
| **IdleHold_PelvisRight_Large** | +Y | Large | 200,000 |

## 4. Diagnostics & Trace

### Structured Logging
For ogni firing event, the system must log:
- `[PhysAnimBalance] TRIGGER: time=%.2f direction=%s magnitude=%s`
- `[PhysAnimBalance] IMPACT: pelvis_vel_pre=(%.1f, %.1f) pelvis_vel_post=(%.1f, %.1f)`

### Pass/Fail Criteria
- **Measurable Response**: Peak pelvis linear velocity > 15 cm/s.
- **Recovered**: Pelvis velocity returns to < 5 cm/s, and root tilt < 8 degrees from vertical, sustained for 0.5s.
- **Failed**: Pelvis height drops below 70cm (fall) or timeout (3s) expires without recovery.
- **Contaminated**: Actor/Capsule moves > 2cm from its start position during the recovery window.

## 5. Trustworthiness Criteria
The framework is considered trustworthy if:
1. Pelvis velocity spikes instantly on impulse application.
2. The actor/capsule remains stationary during the whole trial.
3. Recovery time is proportional to magnitude.
4. "Large" impulses cause a fall if the policy is weak.

# PhysicsControl Cache Prewarm Plan

## Decision

It is still worth continuing in the broader training/runtime alignment direction.

It is not worth continuing the already-falsified sub-directions as the primary lever:
- lower-limb contact-exclusion alignment
- lower-limb target-step caps
- more narrow thigh/calf multiplier reshuffling

The next narrow pass should target PhysicsControl cache warmup and startup ordering.

## Why This Pass

The current movement-smoke baseline is stable, but it still emits a large startup warning burst from `PhysicsControl`:
- `Failed to find bone data for ...`
- `GetCachedBoneTransforms - unable to get bone data for ...`

That warning source is not speculative. In local UE 5.7 source:
- `UPhysicsControlComponent::GetBoneData(...)` warns when `CachedPoseDatas` has no populated `BoneDatas`
- `UPhysicsControlComponent::GetCachedBoneTransforms(...)` warns when cached bone data is unavailable

In the current bridge runtime:
- we manually call `PhysicsControl->UpdateTargetCaches(...)`
- and we also call `PhysicsControl->GetCachedBoneTransforms(...)` every tick while discarding the result

That makes cache readiness and startup ordering a real remaining seam.

## Sources Consulted

### UE online docs

- `UPhysicsControlComponent`
- `UpdateControls`
- `USkeletalMeshComponent`

### Local UE 5.7 source

- [PhysicsControlComponentImpl.cpp](/E:/UE_5.7/Engine/Plugins/Experimental/PhysicsControl/Source/PhysicsControl/Private/PhysicsControlComponentImpl.cpp)
- [PhysicsControlComponent.cpp](/E:/UE_5.7/Engine/Plugins/Experimental/PhysicsControl/Source/PhysicsControl/Private/PhysicsControlComponent.cpp)
- [SkeletalMeshComponent.cpp](/E:/UE_5.7/Engine/Source/Runtime/Engine/Private/Components/SkeletalMeshComponent.cpp)
- [SkeletalMeshComponent.h](/E:/UE_5.7/Engine/Source/Runtime/Engine/Classes/Components/SkeletalMeshComponent.h)

### ProtoMotions docs/code

- ProtoMotions config docs: <https://nvlabs.github.io/ProtoMotions/user_guide/configuration.html>
- ProtoMotions repo: <https://github.com/NVlabs/ProtoMotions>
- [simulator.py](/F:/NewEngine/Training/ProtoMotions/protomotions/simulator/base_simulator/simulator.py)
- [humanoid_utils.py](/F:/NewEngine/Training/ProtoMotions/protomotions/envs/base_env/env_utils/humanoid_utils.py)

## Key Findings

1. PhysicsControl expects cached skeletal pose data to exist before control/body-modifier paths query it.
2. UE itself uses `TickAnimation(0.f, false); RefreshBoneTransforms();` when it needs a valid pose immediately on skeletal mesh initialization.
3. ProtoMotions has no equivalent runtime “bone-cache warmup” seam; this is purely a UE-side representation/order issue.
4. The bridge currently does one useless thing that can amplify warning noise:
   - it calls `GetCachedBoneTransforms(...)` every tick and ignores the result.

## Plan

1. Add a one-shot skeletal pose prewarm right before the first activation-time `PhysicsControl->UpdateTargetCaches(0.0f)`.
2. Skip that prewarm for follower meshes using a leader pose.
3. Remove the unused per-tick `GetCachedBoneTransforms(...)` call.
4. Keep the existing locomotion baseline otherwise unchanged.
5. Verify:
   - build passes
   - `PhysAnim.Component` passes
   - `PhysAnim.PIE.MovementSmoke` passes
   - startup cache-miss warning counts drop materially in the fresh log

## Success Criteria

- no regression in the current locomotion baseline
- material reduction in startup `PhysicsControl` bone-data warnings
- no fail-stop introduced by the prewarm

## Failure Criteria

- warnings stay effectively unchanged
- movement smoke regresses
- activation ordering changes create new startup instability

If this pass fails, the next seam should move to a different locomotion-time representation issue rather than more cache-order tinkering.

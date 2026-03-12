# Locomotion Shell-Coupling Audit Plan

## Purpose

Test whether preserved `ACharacter` shell motion is the main remaining locomotion-time mismatch, or whether the remaining issue still lives primarily in lower-limb target representation / response.

## Why This Audit Exists

The first shell-coupling telemetry pass suggested multi-meter shell/body drift during movement. That was severe enough to justify a pivot away from lower-limb alignment work.

That conclusion was only worth acting on if the telemetry was correct.

## Sources Consulted

### UE online docs

- `ACharacter`: <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/GameFramework/ACharacter>
- `UCharacterMovementComponent`: <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/GameFramework/UCharacterMovementComponent>
- `USkeletalMeshComponent`: <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/Components/USkeletalMeshComponent>
- `UPhysicsControlComponent`: <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl/UPhysicsControlComponent>

### Local UE source

- [CharacterMovementComponent.cpp](/E:/UE_5.7/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp)
- [SkeletalMeshComponent.h](/E:/UE_5.7/Engine/Source/Runtime/Engine/Classes/Components/SkeletalMeshComponent.h)
- [SkeletalMeshComponentPhysics.cpp](/E:/UE_5.7/Engine/Source/Runtime/Engine/Private/SkeletalMeshComponentPhysics.cpp)
- [PhysicsControlComponentImpl.cpp](/E:/UE_5.7/Engine/Plugins/Experimental/PhysicsControl/Source/PhysicsControl/Private/PhysicsControlComponentImpl.cpp)

### Local bridge code

- [PhysAnimComponent.cpp](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp)
- [PhysAnimBridge.cpp](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBridge.cpp)
- [PhysAnimBridge.h](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimBridge.h)

## Audit Rule

Do not pivot away from lower-limb alignment unless corrected shell telemetry still shows large shell/root divergence after fixing any reference-frame errors.

## Corrected Findings

After fixing shell telemetry to use raw root world position and raw root world velocity instead of already shell-relative diagnostics:

- shell planar offset delta is modest, not catastrophic
- shell planar velocity mismatch is modest
- shell/root planar velocity alignment stays high during scripted movement
- the movement smoke path remains stable

## Decision

The shell-coupling audit is now complete.

It was worth doing because it ruled out the wrong path, but it does **not** justify making shell authority the main remaining alignment focus.

## Outcome

Keep the preserved gameplay shell as-is for the current Stage 1 baseline and continue the main alignment work on lower-limb locomotion-time representation / response.

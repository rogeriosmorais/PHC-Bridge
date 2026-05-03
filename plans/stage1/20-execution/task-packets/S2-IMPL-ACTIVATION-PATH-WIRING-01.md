# Task Packet: S2-IMPL-ACTIVATION-PATH-WIRING-01

## Purpose
Wire the proven standing proof logic into the runtime activation path. This involves ensuring that the character only enters `BalanceActive_Standing` after the live proof has been satisfied, and correctly handling failures during the activation wait.

## Allowed Files
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Core.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.PhysicsActivation.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimStandingProof.FunctionalTests.cpp`
- `plans/stage1/20-execution/execution-log.md`
- `plans/stage1/20-execution/task-packets/S2-IMPL-ACTIVATION-PATH-WIRING-01.md`

## Forbidden Files
- ALL other runtime files
- Skeleton or ONNX assets

## Required Work
1. **Activation Wait Wiring**: Update the activation wait logic in `Core.cpp` to check `IsLiveRuntimeEvidenceProofSatisfied()`.
2. **Fast-Forward Logic**: Implement fast-forwarding of simulation handoff alpha and bring-up groups in `PhysicsActivation.cpp` to prevent kinematic demotion on the first physics tick.
3. **Fail-Stop Integration**: Ensure that if the proof fails during the activation wait, the component transitions to `FailStopped`.
4. **Functional Test Verification**: Add `ActivationPath.Wiring` test cases to verify `ProofDisabled`, `ProofNotSatisfied`, `ProofSatisfied`, and `ProofFailed` paths.
5. **Log Expectations**: Handle the high-severity logs from the artifact emitter in the automation test harness using `AddExpectedError`.

## Definition of Done
- `PhysAnimPlugin` builds successfully.
- `ActivationPath.Wiring` functional tests all return `Result={Success}`.
- Character stays in `BalanceActive_Standing` for the full duration after proof satisfaction.
- No kinematic demotion occurs during the first physics tick of activation.

## Stop Conditions
- Build failure.
- Functional test failure.
- Character drops to state 11 (Uninitialized) during the 5s window.

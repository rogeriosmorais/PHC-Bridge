# S2-IMPL-POLICY-INPUT-DESCRIPTOR-CONTRACT-01 Evidence

Base: `503f9c8c0d6e9d83602331cc3267bb6ad72cd4b7`
Head: `6b21fe0487bdecca0878ec9056ba1c8462fde21c`
Commit: `6b21fe0487bdecca0878ec9056ba1c8462fde21c`

Purpose:
- Add a deterministic NNE input descriptor contract before runtime locomotion depends on model input binding.

Implementation summary:
- Added `PhysAnimBridge::ValidateInputTensorDescs`.
- The helper requires exactly `self_obs`, `mimic_target_poses`, and `terrain`; each input must be `Float`, rank 2, batch dimension `1` or `-1`, and the expected feature width.
- Added `PhysAnim.Bridge.InputDescriptorContract` for valid descriptors, reordered descriptors, duplicate/missing/unknown descriptors, non-float descriptors, invalid rank, invalid batch dimension, and invalid width.
- Routed `UPhysAnimComponent::ValidateModelDescriptorContract` through the bridge helper.
- No locomotion state-machine, adapter, termination, validator, support truth, arbitration, asset, ONNX, PoseSearch, mass, or PhysicsControl files were edited.

Command results:
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1`: PASS
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.Bridge.InputDescriptorContract`: PASS
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.Bridge.TensorIndexMap`: PASS
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.Bridge.ActionOutputDescriptorContract`: PASS
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.Bridge.ActionConditioningContract`: PASS
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.LocomotionActiveShell`: PASS

Forbidden files touched: `none`

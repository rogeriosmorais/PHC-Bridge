# S2-IMPL-POLICY-ACTION-OUTPUT-DESCRIPTOR-CONTRACT-01 Evidence

Base: `a1750020817eb372e5e9cdd694a4ff10c03c3be7`
Head: `0e14b2d0f81b8c1ced3b3db1aab75ba452e2567e`
Commit: `0e14b2d0f81b8c1ced3b3db1aab75ba452e2567e`

Purpose:
- Add a deterministic NNE action-output descriptor contract before runtime locomotion depends on policy output.

Implementation summary:
- Added `PhysAnimBridge::ValidateActionOutputTensorDescs`.
- The helper requires exactly one output tensor, `Float` data, rank 2, batch dimension `1` or `-1`, and action dimension `PhysAnimBridge::NumActionFloats`.
- Added `PhysAnim.Bridge.ActionOutputDescriptorContract` for valid and invalid descriptors.
- Routed `UPhysAnimComponent::ValidateModelDescriptorContract` through the bridge helper.
- No locomotion state-machine, adapter, termination, validator, support truth, arbitration, asset, ONNX, PoseSearch, mass, or PhysicsControl files were edited.

Command results:
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1`: PASS
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.Bridge.ActionOutputDescriptorContract`: PASS
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.Bridge.TensorIndexMap`: PASS
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.Bridge.ActionConditioningContract`: PASS
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.Bridge.ActionToBoneMappingContract`: PASS
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.LocomotionActiveShell`: PASS

Forbidden files touched: `none`

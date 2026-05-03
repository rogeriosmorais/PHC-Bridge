# S2-IMPL-POLICY-INFERENCE-FINITE-BUFFER-CONTRACT-01 Evidence

Base: `39a3cddb63c7e17eaf7cd59e3141b4f250674e40`
Head: `3b06041cf5f06dd872c68dfb3d478a041c9d9fe0`
Commit: `3b06041cf5f06dd872c68dfb3d478a041c9d9fe0`

Purpose:
- Add deterministic finite-value validation for all policy inference buffers before running NNE inference.

Implementation summary:
- Added `PhysAnimBridge::ValidateFiniteFloatBuffer`.
- Added `PhysAnim.Bridge.FiniteFloatBufferContract` for finite, NaN, Inf, and named-error cases.
- Routed `UPhysAnimComponent::RunInference` through the helper for `self_obs`, `mimic_target_poses`, `terrain`, and model action output.
- This closes the gap where `terrain` could contain NaN/Inf before `RunSync`.
- No locomotion state-machine, adapter, termination, validator, support truth, arbitration, asset, ONNX, PoseSearch, mass, or PhysicsControl files were edited.

Command results:
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1`: PASS after one test-only compile fix for UE `TNumericLimits<float>` API.
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.Bridge.FiniteFloatBufferContract`: PASS
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.Bridge.InputDescriptorContract`: PASS
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.Bridge.ActionOutputDescriptorContract`: PASS
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.Bridge.ActionConditioningContract`: PASS
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.LocomotionActiveShell`: PASS

Failure classification:
- compile failure
- Cause: test used unavailable `TNumericLimits<float>::QuietNaN()` / `Infinity()` on UE 5.7.
- Fix: test now uses `std::numeric_limits<float>`.

Forbidden files touched: `none`

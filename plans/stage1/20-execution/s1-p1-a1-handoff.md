# S1-P1-A1 Handoff

## Task ID
`S1-P1-A1`

## Decision Summary
- Phase 1 is frozen to the pretrained `motion_tracker/smpl` checkpoint at `F:\NewEngine\Training\ProtoMotions\data\pretrained_models\motion_tracker\smpl\last.ckpt`.
- The Stage 1 ONNX export path is now explicit and test-backed: `F:\NewEngine\Training\scripts\export_onnx.py`.
- The accepted ONNX export uses opset `17` and the locked Stage 1 tensor contract `self_obs=358`, `mimic_target_poses=6495`, `terrain=256`, `actions=69`.
- Offline export validation is complete; the remaining model-side risk is now UE import / NNE runtime creation, not export discovery.

## Produced Artifact
- `Training/physanim/export_onnx.py`
- `Training/scripts/export_onnx.py`
- `Training/tests/test_onnx_export.py`
- `plans/stage1/20-execution/phase1-implementation-package.md`
- `plans/stage1/10-specs/dependency-lock.md`
- `plans/stage1/20-execution/execution-log.md`
- `plans/stage1/20-execution/s1-p1-a1-handoff.md`
- generated offline ONNX: `F:\NewEngine\Training\output\phc_policy.onnx`
- UE import source file: `F:\NewEngine\PhysAnimUE5\Content\NNEModels\phc_policy.onnx`

## What Changed
- Replaced the placeholder ONNX test with concrete checks for the real three-input pretrained actor contract and PyTorch-vs-ONNX parity.
- Added a reusable export module that loads the real PPO actor from the pretrained checkpoint, exports a fixed batch-1 ONNX graph, validates it, and can copy the result into the UE content import path.
- Added the explicit script entry point required by the Stage 1 plan at `Training/scripts/export_onnx.py`.
- Froze the Phase 1 runtime-model decision, the minimal locomotion content scope, the export/import path, and the current `stable enough for G2` definition in the Phase 1 package and lock sheet.

## Dependencies Satisfied
- `G1` is already `pass`.
- The pretrained-versus-fine-tuned runtime choice is now explicit for the current Phase 1 pass.
- The ONNX export/import path no longer requires discovery work before UE runtime validation.
- The deterministic export-side logic is now covered by automated tests before further integration work.

## Open Risks
- Unreal still needs to import `Content/NNEModels/phc_policy.onnx` into a `UNNEModelData` asset and prove NNE runtime creation in-editor.
- `A-06` remains `yellow` until UE-side model load/inference succeeds.
- The exported wrapper targets the inference-time actor path only; if the runtime contract changes away from the locked three-input actor surface, the export path must be revised deliberately.

## Blocked By User?
- no

## Verification
- `F:\NewEngine\Training\.venv\physanim_proto311\Scripts\python.exe -m pytest Training/tests/test_onnx_export.py -v`
- `F:\NewEngine\Training\.venv\physanim_proto311\Scripts\python.exe F:\NewEngine\Training\scripts\export_onnx.py --copy-to-ue`
- Export validation result:
  - accepted opset: `17`
  - parity max abs diff: `1.6391277313232422e-07`
  - parity mean abs diff: `3.3639810936847425e-08`

## Next Consumer
- `S1-P1-A2`

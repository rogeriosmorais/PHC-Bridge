# Stage 1 ONNX Export Spec

## Purpose

This document locks the Stage 1 export path from ProtoMotions checkpoint to Unreal-compatible ONNX model.

It exists so Phase 1 does not have to invent:

- what gets exported
- how the exported model is validated
- what Unreal runtime path it must satisfy

## Export Goal

Produce one ONNX model file that can be imported into Unreal as `UNNEModelData` and executed through `NNERuntimeORT`.

## Official Runtime References

- NNE overview: https://dev.epicgames.com/documentation/en-us/unreal-engine/neural-network-engine-overview-in-unreal-engine
- `NNERuntimeORT` plugin page: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/PluginIndex/NNERuntimeORT
- `INNERuntime` API: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/NNE/INNERuntime

These sources establish:

- Unreal imports ONNX files into `UNNEModelData`
- `NNERuntimeORT` is the ONNX Runtime-backed NNE path
- on Windows, the relevant accelerated path is DirectML-backed ORT

## Locked Stage 1 Export Policy

Stage 1 export must follow these rules:

- export a single-policy inference graph only
- export batch size `1` first
- export `float32` first
- export only the tensors required by the locked bridge contract
- do not include training-only outputs, losses, or debug heads
- do not widen the policy interface just to make export easier

## Source Artifact

The export input is whichever model Phase 1 explicitly chooses:

- pretrained checkpoint, or
- fine-tuned checkpoint

That choice must already be locked in:

- [phase1-implementation-package.md](/F:/NewEngine/plans/stage1/20-execution/phase1-implementation-package.md)
- [dependency-lock.md](/F:/NewEngine/plans/stage1/10-specs/dependency-lock.md)

## Output Artifact

Use these default paths unless Phase 1 writes an explicit replacement:

- local ONNX output: `F:\NewEngine\Training\output\phc_policy.onnx`
- UE import target: `F:\NewEngine\PhysAnimUE5\Content\NNEModels\phc_policy.onnx`

## Export Script Ownership

Stage 1 owns one explicit export entry point:

- preferred script path: `F:\NewEngine\Training\scripts\export_onnx.py`

If Phase 1 uses a different export script or wrapper:

1. record the replacement path in [dependency-lock.md](/F:/NewEngine/plans/stage1/10-specs/dependency-lock.md)
2. update [.agents/workflows/export-onnx.md](/F:/NewEngine/.agents/workflows/export-onnx.md)
3. do not leave the active export path implicit

## Runtime Target In Unreal

Stage 1 runtime preference order:

1. `NNERuntimeORTDml` on Windows for the main path
2. `NNERuntimeORTCpu` only as a correctness/debug fallback if the DirectML path blocks progress

Planning basis:

- Epic documents `NNERuntimeORT` as ONNX Runtime-backed with CPU and DirectML execution providers
- Epic docs also show the CPU runtime example using `NNERuntimeORTCpu`
- Unreal neural profile docs reference `NNERuntimeORTDml`

## Export Validation Contract

Before the ONNX file is accepted for Stage 1, validate all of these:

1. **Shape validation**
   - ONNX input count matches the locked bridge contract
   - ONNX output count matches the locked bridge contract
   - batch dimension behavior is explicit

2. **Numeric validation**
   - run one identical sample through the source model and the ONNX model
   - compare outputs
   - if output mismatch is materially large, reject the export

3. **Unreal import validation**
   - the `.onnx` file imports into a `UNNEModelData` asset
   - the preferred runtime can create a model from it, or the fallback CPU runtime can for debugging

4. **Runtime validation**
   - `SetInputTensorShapes` can be called successfully with the Stage 1 input shape
   - one inference pass runs without immediate runtime failure

## Opset Rule

Use this deterministic export order:

1. try ONNX opset `17`
2. if Unreal import or runtime creation fails due to opset compatibility, retry with `16`
3. if it still fails, retry with `15`
4. record the first accepted opset in [dependency-lock.md](/F:/NewEngine/plans/stage1/10-specs/dependency-lock.md)

This keeps the export path deterministic without pretending the installed engine build has already been tested.

## Export Wrapper Rule

If the raw ProtoMotions model is not directly export-friendly:

- wrap only the inference-time forward path needed by Stage 1
- strip training-only control flow from the export wrapper
- keep wrapper code outside UE

## Failure Rule

If export fails:

1. try the next opset in the deterministic order above
2. if export succeeds but Unreal import fails, test the fallback CPU runtime
3. if both DirectML and CPU runtime paths fail, stop and re-evaluate the Stage 1 model/export path before Phase 1 implementation continues

Do not add a new inference stack.

## Handoff Requirement

The export handoff must include:

- checkpoint source path
- exact export command/script used
- exact ONNX path
- chosen opset
- numeric-validation result
- Unreal import result
- runtime used for validation

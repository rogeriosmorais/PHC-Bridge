# Mandatory Path Forward — Final Execution Record

Date: 2026-07-13

This is an evidence summary, not a product-success declaration. Product authority remains the locked `causal-standing` v1 protocol and its raw UE observations.

## Outcome

The mandatory implementation path is complete. The current ONNX produces valid standing behavior, but the locked product verdict is `FAIL` because it does not provide the required causal recovery advantage over zero actions.

- Source commit evaluated: `9ce67b05a610f602f2462b9118db14fd44996cb2`
- ONNX SHA-256: `c8df64f75e10b3f71766895c054466db1918582ba5a5a85538a1dec8594bbe19`
- Locked protocol SHA-256: `75b29360907028d081cbfa43e965a35fef760873c76d71b52f2147e99f54606a`
- Product bundle: `test-results/product-runs/20260713T191827Z-9ce67b05-e792425a/evaluation.json`
- Product bundle SHA-256: `7fc941bed79d085e987d755f122dc9cf0d2d5f9ff81a1695c0689aecd29026e0`

## Mandatory path audit

1. Observation truth was rechecked and repaired.
   - `361a9c0` made the initial truthfulness corrections.
   - `474aa72` calibrated causal tilt from Manny's neutral pelvis.
   - `a7193f8` records PoseSearch truth independently of policy output.
   - `4f27c24` keeps terrain traces in UE world centimeters while policy tensors remain in policy-space meters.
   - `6528834` restored the bounded Settle readiness thresholds already required by the deterministic contract.

2. The target contract was rechecked and repaired.
   - `302ef41` established parent-relative Physics Control targets and evidence-independent dispatch.
   - `1db550b` references policy actions from Manny neutral.
   - `88ab88e` restored standing control authority.
   - Product target readback is `1.0` in every Normal and ZeroActions repetition.

3. Standing activation was replaced with one atomic ownership path.
   - `5414fe0` added the deterministic activation state model.
   - `871a7c5` added the unified standing Physics Control publisher.
   - `82a7cea` cut standing runtime over to atomic activation.
   - `cf571bf` added Manny standing activation integration coverage.
   - Raw product observations require the pelvis and all critical/support bodies to be valid and simulated, Character Movement inactive, capsule collision disabled, and zero movement reclaim, shell-helper, and topology-change counts.

4. The physical plant passed the fixed five-layer ladder.
   - ControlsOff: `PASS`
   - DampingOnly: `PASS`
   - FixedNeutralTarget: `PASS`
   - ZeroActions: `PASS`, target readback `1.0`
   - RealOnnxPolicy: `PASS`, target readback `1.0`
   - Ladder bundle: `test-results/standing-plant-runs/20260713T181218Z-ba68c3ab-cd5f7791/ladder-summary.json`
   - Ladder bundle SHA-256: `651484e1dc21667fee187f5a8481007516bfd23b30f9082535f776de16005b47`

5. Full policy authority and action responsiveness were verified.
   - The runtime consumes 69 outputs in the 23-joint policy order and maps them to the standing control targets, with the intended distal hand collapse.
   - `ba68c3a` treats steady finite policy output as valid rather than falsely rejecting it.
   - A clean `ActionScale=0.2` experiment (`47c7cb8`) materially worsened the Normal recovery AUC to about `93.85` and failed absolute pelvis/tilt criteria; it was reverted by `fda0001`.
   - The retained `ActionScale=0.1` produces finite nonzero actions, full target readback, and absolute Normal passes.

6. The immutable product ladder ran without protocol changes.
   - Normal AUCs: `38.689077`, `38.492122`, `38.492122`; all three runs `PASS` every absolute criterion.
   - ZeroActions AUCs: `41.141796`, `41.141796`, `41.358093`.
   - Normal/Zero median recovery AUC ratio: `0.9355965`; required maximum: `0.8`.
   - DropControlDispatch: behavioral `FAIL` on `target_readback`, ratio `0.08520465`.
   - ForcedSupportLoss: behavioral `FAIL` on `support_gap`.
   - Final product verdict: `FAIL` only on `causal_recovery_advantage`.

An earlier session at `test-results/product-runs/20260713T191525Z-9ce67b05-d04d9d57` was interrupted by the command wrapper after four runs. It has no bundle evaluation and is therefore malformed/`INVALID`, not product evidence.

## Verification

- `python -m pytest scripts/tests -q`: `87 passed`
- `PhysAnim.Bridge`: all `17` UE automation contracts passed.
- `scripts/test_runtime.ps1`: valid Normal `PASS`, AUC `38.492122`, target readback `1.0`.
- Serena diagnostics on affected C++ files: no errors; two pre-existing unused-include warnings remain in `PhysAnimBridge.cpp`.
- UE logs were read after every executed automation/smoke test.

## Hard limitation reached

The physical plant, observation boundary, NNE inference, Physics Control dispatch/readback, Chaos topology, absolute Normal behavior, and discriminative controls all pass. The existing ONNX nevertheless misses the locked causal advantage by a wide margin: it improves the median recovery AUC by only about `6.44%`, versus the required `20%`.

This checkout contains neither the training checkpoint nor authoritative actuator-preprocessing metadata. The bounded higher-authority experiment made behavior worse. Under the mandatory path's Hard Limitation, the supported conclusion is that this ONNX artifact is incompatible or incapable under the locked causal-standing v1 contract. Further threshold edits or unauthoritative action remapping would be fake success. Progress requires authoritative export/preprocessing data, the original checkpoint, or explicit permission to replace the model.

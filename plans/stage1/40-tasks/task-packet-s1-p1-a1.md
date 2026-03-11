# Task Packet S1-P1-A1

## Task ID

`S1-P1-A1`

## Goal

Turn a passed G1 into an implementation-ready one-character package without reopening Stage 1 planning decisions.

## Frozen Inputs

- `G1` marked `pass`
- [phase1-implementation-package.md](/F:/NewEngine/plans/stage1/20-execution/phase1-implementation-package.md)
- [bridge-spec.md](/F:/NewEngine/plans/stage1/10-specs/bridge-spec.md)
- [ue-bridge-implementation-spec.md](/F:/NewEngine/plans/stage1/10-specs/ue-bridge-implementation-spec.md)
- [retargeting-spec.md](/F:/NewEngine/plans/stage1/10-specs/retargeting-spec.md)
- [test-strategy.md](/F:/NewEngine/plans/stage1/10-specs/test-strategy.md)
- [comparison-sequence-lock.md](/F:/NewEngine/plans/stage1/50-content/comparison-sequence-lock.md)
- [onnx-export-spec.md](/F:/NewEngine/plans/stage1/10-specs/onnx-export-spec.md)
- [acceptance-thresholds.md](/F:/NewEngine/plans/stage1/10-specs/acceptance-thresholds.md)
- current [assumption-ledger.md](/F:/NewEngine/plans/stage1/20-execution/assumption-ledger.md)

## Writable Paths

- [phase1-implementation-package.md](/F:/NewEngine/plans/stage1/20-execution/phase1-implementation-package.md)
- [execution-log.md](/F:/NewEngine/plans/stage1/20-execution/execution-log.md)
- [assumption-ledger.md](/F:/NewEngine/plans/stage1/20-execution/assumption-ledger.md)

## Required Work

1. Freeze whether Phase 1 runs on the pretrained model directly or a fine-tuned derivative.
2. Freeze the one-character bridge scope:
   - observation packing
   - NNE inference
   - action unpacking
   - physics-control writes
   - exact UE runtime owner classes, API calls, and tick/update flow
3. Freeze the ONNX export/import path for the chosen runtime model.
4. Freeze the minimal PoseSearch content required for G2.
5. Freeze the tuning definition for "stable enough for G2."
6. Do not resume bridge code changes until the UE integration spec is explicit enough to remove fresh Unreal API discovery from Phase 1 implementation.

## Definition Of Done

- Phase 1 implementation can begin without new planning work
- the runtime model choice is explicit
- the required code, assets, and tests are scoped tightly enough for one-character delivery

## Escalate To User When

- content or asset choices would expand scope materially
- G1 evidence supports only a narrower thesis than planned

## Verification

- static review against [phase1-implementation-package.md](/F:/NewEngine/plans/stage1/20-execution/phase1-implementation-package.md)
- consistency review against [g2-evaluation.md](/F:/NewEngine/plans/stage1/30-evidence/g2-evaluation.md)
- handoff must use [handoff-format.md](/F:/NewEngine/plans/stage1/00-control/handoff-format.md)

## Next Consumer

- implementation owner for Phase 1
- then `S1-P1-A2`

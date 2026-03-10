# Task Packet S1-P0-A1

## Purpose

This is the first execution-ready AI packet for Stage 1 Phase 0.

Use it only after the user has returned the prerequisite setup evidence described in [user-interventions.md](/F:/NewEngine/plans/stage1/user-interventions.md).

## Task ID

`S1-P0-A1`

## Goal

Turn the planning bundle plus the user's real setup evidence into a finalized Phase 0 execution package with frozen commands, frozen assumptions, and explicit evidence expectations.

## Frozen Inputs

- `AGENTS.md`
- `ENGINEERING_PLAN.md`
- [bridge-spec.md](/F:/NewEngine/plans/stage1/bridge-spec.md)
- [retargeting-spec.md](/F:/NewEngine/plans/stage1/retargeting-spec.md)
- [test-strategy.md](/F:/NewEngine/plans/stage1/test-strategy.md)
- [pretrained-model-selection.md](/F:/NewEngine/plans/stage1/pretrained-model-selection.md)
- [environment-spec.md](/F:/NewEngine/plans/stage1/environment-spec.md)
- [motion-source-map.md](/F:/NewEngine/plans/stage1/motion-source-map.md)
- [ue-project-scaffold.md](/F:/NewEngine/plans/stage1/ue-project-scaffold.md)
- [acceptance-thresholds.md](/F:/NewEngine/plans/stage1/acceptance-thresholds.md)
- the latest user setup evidence

## Writable Paths

- [phase0-execution-package.md](/F:/NewEngine/plans/stage1/phase0-execution-package.md)
- [bridge-spec.md](/F:/NewEngine/plans/stage1/bridge-spec.md)
- [retargeting-spec.md](/F:/NewEngine/plans/stage1/retargeting-spec.md)
- [assumption-ledger.md](/F:/NewEngine/plans/stage1/assumption-ledger.md)
- [execution-log.md](/F:/NewEngine/plans/stage1/execution-log.md)

Do not edit any other Stage 1 planning files in this task unless the orchestrator explicitly expands the writable set.

## Required Work

1. Confirm the real environment details against `environment-spec.md`.
2. Confirm the selected pretrained checkpoint path and eval command against `pretrained-model-selection.md`.
3. Freeze the exact Phase 0 command path and evidence path for:
   - pretrained evaluation
   - bridge-contract confirmation
   - retargeting validation
   - Manny control-path check
   - substep stability check
   - Manny smoke test
4. Update the assumption ledger only where user evidence materially changes confidence.
5. Update the execution log so the next runnable task is accurate.

## Definition Of Done

This task is done only if:

- `phase0-execution-package.md` no longer depends on invented setup assumptions for the user's machine
- the pretrained eval path is concrete enough to run
- bridge and retargeting validation cases are explicitly frozen
- the next worker can start `S1-P0-A2` without re-planning Phase 0

## Escalate To User When

- the real install paths differ materially from the planned environment
- the pretrained checkpoint cannot be obtained or does not match the planned path
- the UE scaffold differs materially from the planned Manny/template structure
- a required dataset or plugin is missing

## Verification

- static review against [phase0-execution-package.md](/F:/NewEngine/plans/stage1/phase0-execution-package.md)
- consistency review against [acceptance-thresholds.md](/F:/NewEngine/plans/stage1/acceptance-thresholds.md)
- handoff must use [handoff-format.md](/F:/NewEngine/plans/stage1/handoff-format.md)

## Next Consumer

- `S1-P0-A2`

# Stage 1 Motion Source Map

## Purpose

This document maps the locked Stage 1 motion set to concrete expected sources so we do not defer content decisions until implementation.

When exact clip decisions are made, record them in [motion-source-lock-table.md](/F:/NewEngine/plans/stage1/50-content/motion-source-lock-table.md).

## Status Meanings

- `broad-pretrained`: expected to be broadly covered by the general pretrained model
- `amass-target`: should be sourced primarily from AMASS-style locomotion data
- `source-risk`: not yet confidently sourceable

## Acquisition Rules

Use these content-selection rules for Stage 1:

- only one upright human performer per clip
- no weapons
- no grappling or partner-dependent interactions
- no flips or acrobatics-heavy moves
- motion should begin and end in a stable stance when possible
- prefer clips that isolate one clear action rather than long choreography

## Minimum Clip Budget

Stage 1 should not start fine-tuning without at least this minimum content plan:

| Motion Family | Minimum Clip Count | Notes |
|---|---|---|
| idle / relaxed stand | 1 | static reference and breathing-weight check |
| walk | 2 | at least one steady forward cycle and one variant |
| jog / run | 2 | enough to test momentum and braking |
| starts / stops | 2 | one start-focused, one stop-focused |
| turns / pivots | 2 | cover left/right or a mirrored pair |
| strafes | 2 | left and right required |
| recovery / rebalance | 2 | short corrective steps, not knockdown recovery |

## Locomotion Core

| Motion | Expected Source | Coverage Status | Notes |
|---|---|---|---|
| idle / relaxed stand | pretrained + AMASS | broad-pretrained | basic stance should be easy to evaluate early |
| walk forward | pretrained + AMASS | broad-pretrained | core feasibility motion |
| jog / run forward | pretrained + AMASS | broad-pretrained | important for momentum check |
| start moving | AMASS locomotion clips | amass-target | choose clips with a clear rest-to-motion transition |
| stop moving | AMASS locomotion clips | amass-target | choose clips with a clear braking phase |
| turn left in place | AMASS locomotion / turning clips | amass-target | use a clean isolated turn, not a long navigation sequence |
| turn right in place | AMASS locomotion / turning clips | amass-target | same as left turn |
| short pivot while moving | AMASS locomotion / turning clips | amass-target | should preserve forward momentum while redirecting |
| strafe left | locomotion side-step clips or approved replacement | source-risk | if unavailable, replace with short lateral step and record that downgrade |
| strafe right | locomotion side-step clips or approved replacement | source-risk | same as left strafe |
| short recovery / rebalance step | broad pretrained motion + selected locomotion clips | source-risk | if unavailable, use a short corrective step after a stop/turn rather than inventing a knockdown flow |

## Planning Implications

- The locomotion core is suitable for pretrained-first feasibility.
- Strafing and rebalance motions are currently the least well-specified parts of the locomotion set and need explicit source confirmation during Phase 0.

## Required Next Confirmation

During execution, this map must be upgraded from source categories to actual clip references:

- exact AMASS subsets or files for locomotion
- explicit note for any motion replaced or removed

## Default Replacement Rules

If the preferred source family is unavailable, replacements must preserve the thesis being tested:

- `strafe` may downgrade to a short lateral step if true strafe data is unavailable
- `recovery / rebalance` may downgrade to a corrective step after stop or turn

## Default Rule If A Motion Is Missing

If a locked motion is not sourceable cleanly:

1. replace it with the closest motion that still tests the same thesis, or
2. remove it explicitly and update the motion set and assumption ledger

Do not silently ignore a missing motion.

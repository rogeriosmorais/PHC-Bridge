# Stage 1 Motion Set

## Purpose

This document locks the motion scope for Stage 1 so the project does not postpone the most important content decision.

## Pretrained Model Starting Point

The current pretrained-first plan assumes we begin from an available ProtoMotions-compatible pretrained model rather than training from scratch immediately.

What that pretrained model is for, in plain terms:

- a **general SMPL humanoid** controller
- broad human motion, not a fighter-specific controller
- trained from broad motion datasets rather than a combat-only dataset

Based on the currently selected pretrained motion-tracker path:

- robot: **SMPL humanoid (no fingers)**
- objective: track future target poses inside simulation
- simulator: **IsaacLab**
- deployment intent: keep the runtime bridge as simple as possible for Stage 1

## What That Means Practically

This is useful for:

- locomotion feasibility
- testing whether the training stack works
- checking whether a pretrained tracker can move in a generally believable way

This is **not** enough to assume:

- clean transfer to UE/Chaos without adaptation
- broad sparse-conditioning behavior like MaskedMimic

So Stage 1 uses this order:

1. evaluate pretrained model first
2. fine-tune it on the locomotion-only Stage 1 motion set if needed
3. train from scratch only if pretrained + fine-tuning still fails

## Locked Stage 1 Motion Categories

### Locomotion Core

These are the motions we want available for Stage 1:

- idle / relaxed stand
- walk forward
- jog or run forward
- start moving
- stop moving
- turn left in place
- turn right in place
- short pivot while moving
- strafe left
- strafe right
- short recovery / rebalance step

## Why This Set

This set is small enough to stay inside Stage 1 scope, but broad enough to test:

- normal body weight transfer
- balance during movement
- turning and braking behavior
- simple recovery after locomotion transitions

It is enough to make G1, G2, and G3 meaningful without turning Stage 1 into a combat-authoring project.

## Explicitly Out Of Scope For Stage 1

Do not expand Stage 1 into:

- grappling
- wrestling / throws
- weapon combat
- punches, kicks, guard poses, and dodge clips as Stage 1 requirements
- flips / acrobatics as a core requirement
- long authored combo trees
- cinematic choreography

## Expected Training/Fine-Tuning Order

1. **Pretrained evaluation**
   - locomotion feasibility
2. **Locomotion adaptation**
   - make sure the controller handles the locomotion core cleanly
3. **Optional post-Stage-1 expansion**
   - only after locomotion quality is acceptable

## Evidence Requirement

Before the orchestrator considers the motion set stable for execution:

- the planned source for each locomotion-core motion must be identified
- any missing motions must be called out explicitly

## Source Expectations

- AMASS should be the default source for locomotion-core motion where possible
- if a motion cannot be sourced cleanly, the orchestrator must either:
  - replace it with a similar motion, or
  - remove it from the locked Stage 1 set explicitly

# Phase 1 Movement Stability Plan

## Purpose

This document freezes the first movement-stability milestone that follows the now-stable passive `BridgeActive` smoke path.

The goal is not to reopen the entire movement architecture at once. The goal is to answer one narrower question first:

- can the active Stage 1 bridge remain stable while the character is driven through a deterministic `WASD`-equivalent movement script in PIE?

## Current Boundary

What is already proven:

- startup succeeds
- passive bridge ownership succeeds
- staged bring-up succeeds
- live policy activation succeeds
- the passive `30` second PIE smoke stays stable

What is not yet proven:

- stability while the character is moving under `WASD`-style gameplay input
- whether the intended product model is:
  - bridge-owned locomotion intent, or
  - coexistence with normal `CharacterMovement`, or
  - a later hybrid

## Frozen First Movement Milestone

The first movement milestone is:

- `BridgeActive` remains stable while a deterministic PIE smoke harness drives the existing third-person movement shell through a scripted `WASD`-equivalent input sequence

This is intentionally narrower than the final gameplay answer.

It is acceptable for this first milestone to run behind a dedicated test-only mode, as long as:

- the default runtime behavior stays unchanged
- the existing passive smoke path stays unchanged
- the movement smoke path is explicit and repeatable

## What This Pass Is

- a deterministic automation harness
- a movement-stability check
- a bridge-plus-gameplay-shell coexistence experiment under controlled conditions

## What This Pass Is Not

- not a change to the locked Stage 1 architecture
- not a commitment that the current default `BridgeActive` gameplay-shell preservation is the final product behavior
- not a replacement for later manual real-keyboard validation
- not a visual-quality or G2 checkpoint

## Frozen Test Mode Rules

The first movement-stability implementation must obey these rules:

1. `BridgeActive` movement preservation must remain explicitly controllable.
   - runtime uses `physanim.AllowCharacterMovementInBridgeActive`
   - movement smoke may also force gameplay-shell preservation for automation
2. Movement testing must still be runnable through dedicated deterministic controls.
3. The movement harness must not depend on real OS keyboard events.
4. The movement harness must be deterministic and runnable through automation.
5. The movement harness must log enough state to interpret failures without relying only on video.

## Deterministic Input Script

The first frozen movement script is measured from the moment the bridge has already reached a stable live-policy baseline.

In practice that means:

- no scripted movement is applied during startup
- no scripted movement is applied during staged bring-up
- no scripted movement is applied during the policy ramp
- the scripted sequence starts only after policy influence has settled

The first frozen movement sequence is:

1. `0-3s`: idle
2. `3-8s`: forward
3. `8-11s`: idle
4. `11-16s`: strafe left
5. `16-19s`: idle
6. `19-24s`: strafe right
7. `24-27s`: idle
8. `27-32s`: backward

The script is expressed as a local-space movement intent:

- forward: `(1, 0)`
- backward: `(-1, 0)`
- left: `(0, -1)`
- right: `(0, 1)`
- idle: `(0, 0)`

## Required Observability

The movement smoke path must log:

- whether movement-smoke mode is active
- current movement-script phase
- current scripted local intent
- current scripted world intent
- owner/world velocity or displacement evidence
- normal bridge runtime diagnostics

If movement fails, the logs must be able to distinguish:

- no movement happened
- movement happened but destabilized the bridge
- the bridge stayed stable but the gameplay shell did not respond

## Current Status

Latest result on `March 11, 2026`:

- the dedicated movement-smoke harness is implemented
- the passive smoke path remains green
- the passive idle smoke now also passes at `65` seconds without delayed drift or collapse
- movement is now held off until after live policy has settled
- the first true forward movement window still destabilizes the bridge within about `0.26s`

So the harness is now considered valid, but the movement milestone is not yet passed.

## Success Criteria

The first movement-stability gate passes only if all of these are true:

1. the full scripted sequence completes
2. the bridge does not enter `FailStopped`
3. no catastrophic per-body spikes appear in runtime diagnostics
4. the owner actually translates in the commanded directions during the scripted windows
5. the passive smoke path remains green after the movement harness is added

## Implementation Order

The movement-stability work must proceed in this order:

1. add the frozen movement plan and link it into Stage 1 control docs
2. add a dedicated movement-smoke mode behind test-only console variables
3. preserve the gameplay shell only when that movement-smoke mode is active
4. apply the deterministic movement script during `BridgeActive`
5. extend runtime diagnostics with movement-smoke evidence
6. add a separate PIE automation test for movement smoke
7. keep the original passive smoke test intact
8. rebuild, run component automation, run passive smoke, run movement smoke

## Escalation Rules

Escalate only if:

- the movement-smoke harness cannot produce actual movement without reopening asset or Blueprint wiring
- preserving the gameplay shell in test mode invalidates the passive smoke path
- failures point to a product-level choice about movement ownership rather than a local stabilization issue

If the first movement smoke passes, the next follow-up becomes:

- short manual verification with real `WASD`

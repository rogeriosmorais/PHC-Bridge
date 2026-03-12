# Lower-Limb Target-Range Policy Plan

## Purpose

After the verified `0.50` toe-limit baseline, the next question was whether the remaining lower-limb movement spikes came from malformed ankle authoring or from policy targets repeatedly pushing against Manny's narrow lower-limb envelope.

The new runtime instrumentation answers that:

- direct ankle authoring is structurally sane
- the policy is repeatedly over-occupying the lower-limb limit proxy, especially at the calf/knee chain

## Current Evidence

From the latest verified movement smoke:

- direct ankle constraints are present and symmetric:
  - `foot_l <- calf_l`
  - `foot_r <- calf_r`
- first policy-enabled frame:
  - `lowerLimbLimitOccupancy=calf_l:0.00x proxy=5.0deg`
- once locomotion starts:
  - lower-limb occupancy repeatedly climbs above `1.0x`
  - common measured range is about `1.7x - 2.6x`
  - dominant bone is repeatedly `calf_l` or `calf_r`
- the strongest remaining movement spikes still occur in the same chain:
  - `calf_*` linear peaks
  - `ball_*` angular peaks

## Interpretation

The current Manny lower-limb chain is not grossly malformed.

The more likely remaining mismatch is:

- ProtoMotions lower-limb target semantics are being interpreted too broadly for Manny's effective calf/knee envelope
- the policy keeps asking for lower-limb rotations that are reasonable in training but effectively over-occupy Manny's narrow lower-limb limits during locomotion
- the toe chain then becomes a visible downstream offender, but the calf/knee target-range mismatch is earlier in the chain

## Next Runtime Pass

Do not retune toe authoring again.

Do not broaden Manny hard limits blindly.

Instead, add a temporary Stage 1 lower-limb target-range policy:

1. lower-limb only
   - `thigh_*`
   - `calf_*`
   - `foot_*`
   - `ball_*`
2. scale raw policy target rotations toward the current baseline before writing control targets
3. start with the most likely high-value subset:
   - `calf_*` first
   - optionally `foot_*` second
4. judge success only on deterministic movement smoke

## Success Criteria

The first lower-limb target-range pass is good if:

- build passes
- `PhysAnim.Component` passes
- `PhysAnim.PIE.MovementSmoke` passes
- no fail-stop
- lower peak body linear speed than the current `0.50` toe-limit baseline
- lower peak body angular speed than the current `0.50` toe-limit baseline
- lower-limb limit occupancy falls materially, especially for `calf_*`

## Decision Rule

- If calf-only target-range scaling improves both occupancy and movement spikes:
  - keep it as the next baseline candidate
- If it reduces occupancy but worsens movement behavior:
  - move the next pass to `foot_*` or a different lower-limb representation surface
- If it does not reduce occupancy meaningfully:
  - the next mismatch is not target amplitude first and we should inspect lower-limb representation / frame semantics instead

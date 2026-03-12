# Unreal Insights Bridge Instrumentation Plan

## Why this pass

The bridge trace output now tells us what the bridge packed and decided on each sampled policy step, but it does not show where those bridge phases sit relative to:

- game thread work
- animation work
- PhysicsControl cache/control updates
- Chaos and frame timing

For that we need first-class Unreal trace instrumentation so Timing Insights can correlate bridge spikes with engine spikes.

## Sources checked

### Unreal docs

- `UPhysicsControlComponent`
- `UpdateControls`
- Unreal Insights / Timing Insights documentation

### Local UE source

- `PhysicsControlComponent.cpp`
- `PhysicsControlComponentImpl.cpp`
- existing engine/plugin examples using:
  - `TRACE_CPUPROFILER_EVENT_SCOPE`
  - `TRACE_COUNTER_SET`
  - `TRACE_BOOKMARK`

### ProtoMotions

- active SMPL checkpoint config
- `humanoid_obs.py`
- `mimic_obs.py`
- `terrain_obs.py`

ProtoMotions remains the source of the packed model contract. Unreal Insights is the source of temporal correlation against engine systems.

## Plan

1. Add CPU profiler scopes around bridge work that matters at policy-step time:
   - PoseSearch query
   - future pose sampling
   - observation packing
   - `RunSync`
   - control writes
2. Add counters for the bridge timings and a few bridge spike signals:
   - PoseSearch query ms
   - future pose sample ms
   - observation pack ms
   - `RunSync` ms
   - control writes ms
   - update controls ms
   - max body angular speed
   - max lower-limb occupancy
   - number of written policy targets
3. Add bookmarks for fail-stop so they are visible directly in Timing Insights.
4. Keep the existing CSV trace output. This pass is additive, not a replacement.
5. Rebuild and rerun:
   - `PhysAnim.Component`
   - `PhysAnim.PIE.MovementSmoke`
6. Document the new Insights surface in the execution docs.

## Success criteria

- build is green
- smoke is green
- bridge scopes appear as first-class Timing Insights slices
- bridge counters/bookmarks are emitted without changing runtime behavior

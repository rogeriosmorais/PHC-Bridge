# GPU-Native Animation Engine - Engineering Plan (v9)

## 1. Objective

Build a real-time Unreal Engine demo where physics simulation is the animation system, in two stages:

- **Stage 1 (Proof of Quality):** prove physics-driven motion looks fundamentally better than kinematic animation, using mostly UE5 built-in systems plus a minimal PHC bridge.
- **Stage 2 (GPU Migration):** move the articulated-body simulation from CPU to GPU compute once Stage 1 proves the animation thesis.

Hardware target:

- Intel i7-14700
- RTX 4070 SUPER

## 2. Why the project is split into two stages

The project is answering two different questions:

1. does physics-driven animation look better than kinematic animation?
2. if yes, is that simulation worth migrating to the GPU?

Stage 1 exists to answer the first question cheaply and rigorously. Stage 2 is only justified if Stage 1 succeeds.

---

## 3. Stage 1 architecture

```text
UE5 PoseSearch (CPU)
  -> locomotion reference pose / target context
PHC policy via NNE (GPU)
  -> desired joint-relative orientation targets
UPhysicsControlComponent (CPU)
  -> control target orientations + angular gains
Chaos Physics (CPU)
  -> articulated rigid-body simulation
UPhysicalAnimationComponent (optional / selective)
  -> visual blending / authoring support where needed
UE5 rendering + standard gameplay systems
```

### Stage 1 custom code remains intentionally small

The custom Stage 1 bridge should stay concentrated in `UPhysAnimComponent` and related plugin code.

The custom bridge is responsible for:

1. gathering runtime body state
2. packing model observations
3. running PHC inference through NNE
4. unpacking actions into target orientations
5. writing those targets into the runtime control layer
6. managing the runtime state machine and balance-entry state machine

The bridge is not supposed to replace Chaos, Physics Control, or general engine physics infrastructure.

---

## 4. Stage 1 architecture direction: transitional now, balance-first later

### 4.1 Current practical architecture

Right now the project still has two sequential runtime chains:

1. **normal bridge startup / stabilization bring-up**
2. **balance-entry conversion**

This is acceptable during Stage 1 stabilization because it isolates bridge bootstrapping from balance-entry contract debugging.

### 4.2 Long-term architecture target

The final target is **always-on balance**.

That means balance should eventually become the normal runtime condition, not a special mode layered on top of a separate long-lived `BridgeActive` world.

The likely long-term shape is:

- balance startup
- balance settle
- balance active
- balance recovery
- safe deny / fallback

The current split should therefore be treated as a transitional scaffold, not the final architecture.

### 4.3 Practical rule for Stage 1

Do not rewrite into a single balance-first architecture yet.

First get the current balance-entry path contract-correct and physically credible enough to prove or falsify the Stage 1 thesis.

Then collapse the architecture later if Stage 1 succeeds.

---

## 5. Stage 1 bridge problem statement

The bridge problem has two different layers and they must stay separate.

### 5.1 Contract correctness

The bridge is contract-correct when:

- request / acceptance behavior is correct
- runtime states are explicit and truthful
- accepted Phase 1 topology is explicit and preserved
- normal policy writes and hold-path writes are separated correctly
- convergence checks use authoritative post-update telemetry
- freeze lifetime covers the full Phase 1 attempt
- terminal outcomes are specific and truthful

### 5.2 Physical viability

The bridge is physically viable when:

- the accepted frozen Phase 1 setup remains dynamically quiet enough to survive Prepare and LateValidate
- contact behavior does not immediately destabilize the sim set
- tuning / target writes do not inject unacceptable energy
- the accepted setup has enough stability margin to continue into later balance phases

A Phase 1 attempt can be contract-correct and still physically non-viable.

That is exactly where the project currently is.

---

## 6. Current Stage 1 conclusion

The project is no longer mainly blocked by ambiguous state-machine behavior.

The current central hypothesis is now:

> Is the accepted Phase 1 frozen setup physically viable under the current control, tuning, and contact conditions?

That hypothesis is not yet proven.

Current logs indicate that the accepted Phase 1 setup can be contract-correct and still fail very quickly due to sim-body instability.

This is progress, not regression, because the project can now distinguish architectural mistakes from physical viability failure.

---

## 7. Stage 1 runtime contract summary

Stage 1 must preserve these high-level rules:

- normal bridge startup may use staged non-root bring-up
- balance entry is a separate runtime contract layered on top of a running bridge
- balance entry must leave plain `BridgeActive`
- Phase 1 uses a dedicated accepted topology snapshot
- Prepare and LateValidate are hold-only from the control-write side
- convergence and admission checks use cached post-update telemetry
- safe denial is an explicit valid terminal outcome
- the balance smoke must never silently end in plain `BridgeActive`

The detailed contract is frozen in the Stage 1 spec files under `plans/stage1/10-specs/`.

---

## 8. Current accepted Phase 1 topology

Under the current design the accepted Phase 1 topology is:

- root: kinematic
- proximal set: simulated
- distal set: simulated
- upper body: kinematic

This means the current design does **not** require pelvis/root simulation as an entry condition.

It does require:

- valid root/pelvis-side body source
- valid uprightness source
- correct ownership snapshot
- sufficient dynamic stability margin

---

## 9. Current unresolved Stage 1 risk

The main unresolved risk is not “can the state machine transition.”

The main unresolved risk is:

- whether the accepted Phase 1 frozen setup is dynamically stable enough to survive LateValidate

Likely contributors include:

- floor contact impulses at the distal chain
- tuning / gain choices
- hold-set scope
- admission thresholds that are still too weak
- the topology itself being physically too aggressive under current conditions

---

## 10. What not to revisit casually

The following areas are now provisionally settled and should not be reopened without new evidence:

- hold-path vs normal policy-write separation
- freeze lifetime contract
- authoritative root-tilt source correction
- post-update convergence snapshot timing
- pelvis simulation as a required gate under `root=kin`

The remaining work should focus on physical viability, not rebreaking solved contract behavior.

---

## 11. Stage 1 document authority

Stage 1 uses a strict hierarchy:

1. `plans/stage1/10-specs/*`
2. `STAGE1_PLAN.md`
3. `plans/stage1/40-design/*`

`10-specs` defines the authoritative runtime contract.

`STAGE1_PLAN.md` defines execution focus and interpretation rules.

`40-design` may explain implementation sequencing, but it may not introduce a runtime contract absent from `10-specs`.

---

## 12. Development phases

### Phase 0: feasibility

Goals:

- lock the PHC observation/action contract
- prove the bridge can write usable targets into UE
- verify SMPL <-> UE mapping is sane
- verify Chaos + Physics Control can be driven stably enough for Stage 1 experiments

### Phase 1: one-actor bridge runtime

Goals:

- make the bridge runtime alive and measurable
- make the balance-entry state machine explicit
- make the balance smoke end in either active balance or safe deny
- eliminate ambiguous outcomes

### Phase 2: physical viability pass

Goals:

- test whether the accepted Phase 1 setup is viable
- tighten admission / deny logic around real body-motion margins
- revise contact assumptions, tuning, or topology only if logs prove the current setup is non-viable

### Phase 3: Stage 1 presentation

Goals:

- produce a convincing proof-of-quality demo
- compare the physics-driven path against kinematic motion clearly enough to justify or reject Stage 2

---

## 13. Decision gates

### G1 — early feasibility

Stop if:

- the bridge contract cannot be locked cleanly
- target writes cannot drive the runtime meaningfully
- the mapping is obviously wrong
- articulated-body control is obviously unusable

### G2 — contract-correct balance entry

Do not call Stage 1 “working” until:

- request / accept path is explicit
- topology is explicit
- suppression contract is correct
- convergence snapshot is authoritative
- freeze lifetime is correct
- terminal outcomes are specific and truthful

### G3 — physical viability

Do not call Phase 1 viable until:

- Prepare → LateValidate admission uses a real stability margin
- LateValidate survives without immediate body-motion deny
- the accepted sim set does not explode as soon as the frozen setup is tested

### G4 — Stage 1 thesis

Proceed to Stage 2 only if the final physics-driven result is convincingly better than the kinematic baseline.

---

## 14. Performance budget

Stage 1 still targets a comfortable 60 FPS budget on the listed hardware.

The existing budget assumptions remain directionally valid, but the more important Stage 1 question remains animation quality and balance viability, not final micro-optimization.

---

## 15. Risk register update

### Previously dominant risks

- balance entry hidden inside ambiguous `BridgeActive`
- no dedicated topology source of truth
- hold/path semantics blurred with policy writes
- stale telemetry driving admission
- freeze released at the wrong time

Those are now substantially reduced.

### Currently dominant risk

- the accepted frozen Phase 1 setup may simply not be viable under present contact/tuning conditions

This is the main engineering question now.

---

## 16. Long-term note

If Stage 1 succeeds, the project should gradually collapse toward a single balance-first runtime instead of preserving a permanent split between generic bridge runtime and balance runtime.

For now, finish the current path just enough to prove or falsify the balance thesis.

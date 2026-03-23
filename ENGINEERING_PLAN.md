# GPU-Native Animation Engine - Engineering Plan (v9)

## 1. Objective

Build a real-time demo in **Unreal Engine 5** where **physics simulation is the animation system**, in two stages:

- **Stage 1 (Proof of Quality):** Prove physics-driven locomotion looks fundamentally better than kinematic animation, using mostly UE5 built-in systems plus a small bridge.
- **Stage 2 (GPU Migration):** Move physics from CPU to GPU compute shaders once Stage 1 proves the visual and control thesis.

**Hardware:** Intel i7-14700 + RTX 4070 SUPER.

### Why Two Stages

1. **Animation quality** — does physics-driven motion look better?
2. **GPU utilization** — is GPU execution worth the added complexity?

Do not pursue Stage 2 unless Stage 1 succeeds.

---

## 2. Stage 1 Architecture (all UE5 built-in, small custom bridge)

```text
Unreal Engine 5.7.x

UE5 PoseSearch (CPU)
  -> target motion context / reference pose
PHC / ProtoMotions policy via NNE (GPU)
  -> desired joint-relative action output
UPhysicsControlComponent (CPU, Experimental)
  -> orientation and angular control targets
Chaos Physics (CPU)
  -> articulated body simulation, contact, friction, collision
Optional UPhysicalAnimationComponent / visual blending
  -> visual transition support only where explicitly allowed
UE5 Renderer
```

### Important Stage 1 reality

Stage 1 is **not** “turn on physics and get stable humanoid balance for free.”

It is a **hybrid articulated control problem** built on:

- Chaos articulated bodies
- Physics Control targets
- contact-rich foot interactions
- mixed kinematic/simulated ownership
- staged authority transfer
- model inference + retargeting

UE provides the pieces, but not a complete stable humanoid-balance stack. The docs and implementation must therefore treat balance entry as a strict contract and a physical-viability experiment, not as a trivial engine feature.

### Engine-grounded constraints

These points must be treated as part of the Stage 1 design basis:

- `UPhysicsControlComponent` is **Experimental** in UE.
- Physics **sub-stepping materially affects** ragdoll and complex articulated stability.
- `Kinematic` vs `Simulated` is a real ownership boundary, not a cosmetic tuning choice.
- Constraint damping and body damping are not universal “make it stable” knobs; they act in different ways and do not replace a correct ownership/control design.

### The only custom bridge code

A small C++ plugin (`PhysAnimPlugin`) that each frame:

1. gathers authoritative body state
2. packs the observation tensors expected by the selected policy
3. runs NNE inference
4. unpacks the model action output
5. writes the resulting control targets into the UE runtime path

The plugin also owns the balance-entry runtime contract and its transition diagnostics.

---

## 3. Stage 1 bridge problem statement

The bridge has two separate questions:

### A. Contract correctness
Can Unreal reproduce the exact runtime ownership, write-routing, and convergence contract that the selected Stage 1 path requires?

### B. Physical viability
Even if the contract is correct, is the accepted Phase 1 frozen setup physically viable under current:

- control tuning
- contact behavior
- sub-step regime
- topology ownership
- hold/reference behavior

These questions must stay separate in both code and docs.

---

## 4. Current Stage 1 balance-entry interpretation

The long-term goal is **always-on balance**.

That means the current architecture should be treated as **transitional**:

- normal bridge startup / bring-up gets the runtime alive
- balance entry converts that live runtime into a frozen Phase 1 contract

This is acceptable for now, but not the final architecture.

Long-term target:

```text
balance startup
-> balance settle
-> balance active
-> balance recovery / safe deny
```

Not:

```text
generic bridge runtime
-> special balance mode layered on later
```

---

## 5. Current Stage 1 Phase 1 contract summary

Under the current design, the accepted Phase 1 topology is:

- `root = kinematic`
- `proximal = simulated`
- `distal = simulated`
- `upper = kinematic`

This means:

- pelvis/root may remain kinematic in Phase 1
- `pelvisSimulating=false` is not, by itself, a Phase 1 failure
- normal policy writes must be suppressed over the accepted Phase 1 set
- only the explicit hold path may publish to allowed kinematic bones
- Prepare / LateValidate decisions must use a **post-update authoritative convergence snapshot**
- freeze lifetime must cover the **full Phase 1 attempt**

The current unsolved question is no longer primarily state-machine ambiguity. It is whether this accepted frozen Phase 1 setup is physically viable.

---

## 6. Development guidance

### Phase 0 / Phase 1 rules

Before any further large refactor, preserve the following distinction:

- **solved / mostly solved:** queueing, explicit acceptance, hold-vs-policy separation, freeze lifetime, root tilt source correction, post-update convergence snapshot timing
- **unsolved:** physical viability of the accepted Phase 1 setup

Do not re-open solved contract areas casually.

### Evidence quality rule

A “successful-looking” run is not enough. Stage 1 evidence must state:

- sub-step regime used
- topology at each entry phase
- whether policy writes were suppressed
- whether hold-only writes remained
- which body set produced the worst motion
- whether failure was contract-level or physical-level

---

## 7. Key risks

| Risk | Likelihood | Impact | Notes |
|---|---:|---:|---|
| Sim-to-sim gap (training simulator -> Chaos) | High | High | Likely permanent tuning burden |
| Phase 1 contract correct but physically non-viable | High | High | Current leading hypothesis |
| Foot/contact instability dominates distal chain | High | High | Likely major factor in entry failure |
| Physics Control limitations / Experimental behavior | Medium | Medium | Must not be treated as a black-box stable motor system |
| Over-constrained kinematic hold set destabilizes sim set | Medium | High | Needs evidence-driven review |
| Weak admission margin allows LateValidate to start too early | Medium | Medium | Should be treated separately from deny thresholds |

---

## 8. Acceptance view for Stage 1

Stage 1 balance entry is only truly “working” when all are true in the same run:

- request / preflight / entry state machine are correct
- accepted topology is correct
- suppression and hold-only semantics are correct
- freeze lifetime is correct
- post-update convergence snapshot is correct
- Prepare -> LateValidate admission requires real stability margin
- LateValidate survives long enough to show the accepted setup is physically viable
- the runtime can either activate or deny safely with truthful reasons

Until then, Stage 1 remains an active balance-entry investigation rather than a solved runtime.

---

## 9. Project structure

```text
NewEngine/
|-- ENGINEERING_PLAN.md
|-- STAGE1_PLAN.md
|-- plans/stage1/10-specs/
|   |-- bridge-spec.md
|   |-- ue-bridge-implementation-spec.md
|   `-- balance-mode-entry-spec.md
|-- plans/stage1/40-design/
|   |-- balance-perturbation-mode-design.md
|   |-- balance_mode_entry_transition_spec.md
|   |-- balance_mode_phase1_stabilization_spec.md
|   |-- balance_mode_phase2.md
|   `-- ankle-constraint-authoring-audit-plan.md
`-- PhysAnimUE5/
    `-- Plugins/PhysAnimPlugin/
```

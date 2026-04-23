# PHC-Bridge Engineering Plan (v11)

## 1. Objective

Build a real-time demo in **Unreal Engine 5** where **physics simulation is the animation system**, in two stages:

- **Stage 1:** prove a small UE bridge can drive a physics-based humanoid from a PHC-family policy and reach sustained physical standing.
- **Stage 2:** consider GPU-side physics migration only if Stage 1 proves the visual and control thesis.

**Hardware:** Intel i7-14700 + RTX 4070 SUPER.

Do not pursue Stage 2 unless Stage 1 succeeds.

---

## 2. Stage 1 Architecture

```text
Unreal Engine 5.7.x

UE5 PoseSearch
  -> target motion context / reference pose
PHC / ProtoMotions policy via NNE
  -> desired joint-relative action output
UPhysicsControlComponent
  -> control targets and authority ramp
Chaos Physics
  -> articulated body simulation, contact, friction, collision
UE5 Renderer
```

The Stage 1 bridge stays small:

1. gather authoritative runtime state
2. pack observations
3. run NNE inference
4. unpack actions
5. publish control targets and activation diagnostics

---

## 3. Active Direction

Stage 1 now uses a **balance-first activation** design.

The target runtime is not a flip-based ritual with a later certified handoff. The target runtime is:

```text
BridgeActive_Physical
-> BalanceActivation_BlendIn
-> BalanceActivation_StandingValidation
-> BalanceActive_Standing
-> BalanceActive_Recovery / SafeDenied / Failed
```

Core rules:

- the balance-critical chain stays continuously simulated through activation
- controller authority ramps onto an already-physical state
- topology and ownership flips must be minimized
- diagnostics measure reality and must not manufacture a pass
- success is only sustained `BalanceActive_Standing` for `3.0` continuous seconds

The default balance-critical chain for Stage 1 is:

- `pelvis`
- `spine_01`
- `spine_02`
- `spine_03`
- `thigh_l`
- `thigh_r`

Distal and upper-body ownership may still be tuned, but the balance-critical chain must not rely on temporary kinematic re-ownership during activation.

---

## 4. Contract And Viability

The bridge still has two separate questions.

### Contract correctness

Can Unreal reproduce the required runtime contract for:

- continuous simulated ownership of the balance-critical chain
- gradual controller-authority blend
- truthful shell bookkeeping versus shell influence separation
- explicit terminal outcomes

### Physical viability

Even if the contract is correct, can the live physical system survive activation under current:

- control tuning
- contact behavior
- sub-step regime
- shell behavior
- balance-critical ownership
- controller blend timing

These questions must stay separate in code, docs, and evidence.

---

## 5. Development Guidance

Preserve these solved or intentional distinctions:

- intended ownership, modifier-record ownership, and raw body state are different observables
- shell bookkeeping and shell influence are different observables
- diagnostics are allowed to classify failure, not to widen grace until the run appears successful
- truthful safe deny is useful telemetry but never a passing outcome

Evidence for any standing claim must state:

- sub-step regime
- balance-critical ownership continuity
- controller-authority blend behavior
- whether policy/control authority was blended or abruptly applied
- whether shell influence was absent or materially active
- worst-body or worst-family instability
- whether the failure was contract-level or physical-level

---

## 6. Key Risks

| Risk | Likelihood | Impact | Notes |
|---|---:|---:|---|
| Sim-to-sim gap (training simulator -> Chaos) | High | High | Likely persistent tuning burden |
| Balance-critical ownership continuity is not achievable with current UE path | High | High | Main structural risk after the direction change |
| Controller blend-in destabilizes an already-physical state | High | High | New primary activation risk |
| Shell bookkeeping is correct but shell influence is still materially active | High | High | Must be measured, not explained away |
| Hidden ownership flips remain in the runtime | Medium | High | Would violate the target activation contract |
| Physics Control limitations / Experimental behavior | Medium | Medium | Must not be treated as a turnkey balance stack |
| Standing validation passes briefly but does not sustain | Medium | High | Product failure even if activation looks promising |

---

## 7. Acceptance View For Stage 1

Stage 1 balance activation is only truly working when all are true in the same run:

- the bridge enters `BridgeActive_Physical`
- the balance-critical chain remains continuously simulated through activation
- controller authority ramps gradually onto the already-physical chain
- diagnostics stay observational only
- no hidden shell or gameplay correction manufactures a pass
- the run reaches `BalanceActive_Standing`
- `BalanceActive_Standing` persists continuously for `3.0` seconds

Until then, Stage 1 remains an active balance-activation investigation.

---

## 8. Project Structure

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
|   |-- balance_mode_phase3_settle.md
|   |-- phase1-late-validate-truth-model.md
|   |-- phase1-transactional-auto-calibration-harness.md
|   `-- phase2-rooton-truth-model.md
`-- PhysAnimUE5/
    `-- Plugins/PhysAnimPlugin/
```

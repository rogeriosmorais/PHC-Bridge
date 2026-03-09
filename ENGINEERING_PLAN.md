# GPU-Native Animation Engine — Engineering Plan (v8 — Final)

## 1. Objective

Build a real-time demo of two human fighters in **Unreal Engine 5** where **physics simulation is the animation system**, in two stages:

- **Stage 1 (Proof of Quality):** Prove physics-driven animation looks fundamentally better than kinematic animation, using almost entirely UE5 built-in systems. **~6 weeks, ~200 lines of custom C++.**
- **Stage 2 (GPU Migration):** Move physics from CPU to GPU compute shaders, proving idle GPU resources should be used for animation. **~7 additional weeks.**

**Hardware:** Intel i7-14700 + RTX 4070 SUPER.

### Why Two Stages

1. **Animation quality** (does physics-driven motion look better?) — risky, tested cheaply in Stage 1.
2. **GPU utilization** (should the GPU run this?) — only worth pursuing if Stage 1 succeeds.

---

## 2. Stage 1 Architecture (All UE5 Built-In)

```
┌────────────────────────── Unreal Engine 5.5+ ──────────────────────────┐
│                                                                         │
│  UE5 PoseSearch (CPU)                                                  │
│    │  target joint angles/velocities from mocap database               │
│    ▼                                                                    │
│  PHC Policy via NNE (GPU, ~0.4ms)         ◄── CUSTOM (~200 lines)     │
│    │  desired joint orientations + stiffness                           │
│    ▼                                                                    │
│  UPhysicsControlComponent (CPU)           ◄── BUILT-IN                 │
│    │  SetControlTargetOrientation() per bone                           │
│    │  SetControlAngularData() for strength/damping                     │
│    │  Spring/damper motor drives each joint                            │
│    ▼                                                                    │
│  Chaos Physics (CPU)                      ◄── BUILT-IN                 │
│    │  Articulated ragdoll simulation                                   │
│    │  Gravity, ground contact, friction, inter-character collision      │
│    ▼                                                                    │
│  UPhysicalAnimationComponent (CPU)        ◄── BUILT-IN                 │
│    │  Physics ↔ animation blend weights                                │
│    │  Smooth transitions (e.g., knockdown → recovery)                  │
│    ▼                                                                    │
│  Chaos Flesh + ML Deformer (CPU+GPU)      ◄── BUILT-IN                 │
│    │  Volumetric flesh deformation (visual bonus)                      │
│    ▼                                                                    │
│  UE5 Renderer (Nanite/Lumen)              ◄── BUILT-IN                 │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### The Only Custom Code (~250 lines)

A C++ plugin (`PhysAnimPlugin`) that each frame:

1. **Gathers body state** from the Physics Asset (~131 floats: joint orientations, angular velocities, center-of-mass, ground contacts).
2. **Retargets** UE5 skeleton state → SMPL observation vector (static joint mapping table, ~50 lines).
3. **Runs PHC inference** via UE5 NNE (ONNX Runtime GPU) — outputs ~106 floats (desired orientations + stiffness per joint).
4. **Retargets** SMPL output → UE5 bone targets (inverse mapping).
5. **Writes results** to the `UPhysicsControlComponent` via `SetControlTargetOrientation()` and `SetControlAngularData()`.

That's it. PD control, collision, contact, blend transitions, flesh deformation, rendering — all built-in UE5.

### Skeleton Retargeting (SMPL ↔ UE5)

PHC is trained on the SMPL skeleton (24 joints). The UE5 character uses the UE5 mannequin skeleton (Manny/Quinn, ~70 bones). The plugin needs a static mapping between them:

| SMPL Joint | UE5 Bone | Notes |
|---|---|---|
| Pelvis | pelvis | Root — position + orientation |
| L_Hip / R_Hip | thigh_l / thigh_r | |
| L_Knee / R_Knee | calf_l / calf_r | |
| L_Ankle / R_Ankle | foot_l / foot_r | |
| Spine1–3 | spine_01–03 | May need interpolation |
| L_Shoulder / R_Shoulder | upperarm_l / upperarm_r | |
| L_Elbow / R_Elbow | lowerarm_l / lowerarm_r | |
| L_Wrist / R_Wrist | hand_l / hand_r | |
| Head / Neck | head / neck_01 | |

Coordinate system conversion (SMPL uses Y-up, UE5 uses Z-up) is handled once in the mapping. The UE5 skeleton has more bones than SMPL (fingers, twist bones, etc.) — unmapped bones retain their animated pose from PoseSearch.

### Physics Substep Configuration

Articulated bodies driven by PD controllers need 120–240 Hz physics to be stable. Chaos Physics supports this via two independent mechanisms:

**Option A: Synchronous Substepping** (simpler, recommended for Stage 1)
- Enable `Substepping` in Project Settings → Engine → Physics
- Set `Max Substep Delta Time` = 0.004167s (240 Hz) or 0.008333s (120 Hz)
- Set `Max Substeps` = 4–8
- At 60 FPS render, each frame runs 2–4 physics substeps
- PHC inference runs once per render frame; torque targets are held constant across substeps

**Option B: Async Physics** (more robust, if Option A is unstable)
- Enable `Tick Physics Async` in Project Settings
- Set `Async Fixed Time Step Size` = 0.004167s (240 Hz)
- Physics runs on a separate thread at a fixed rate, decoupled from rendering
- Game thread and physics thread communicate via input/output buffers
- Caveat: `Substepping` and `Tick Physics Async` are mutually exclusive

**If Chaos substeps are insufficient (instability persists):**
- Increase `Velocity Iterations` and `Position Iterations` in the Physics Asset constraints
- Reduce `Max Depenetration Velocity` to prevent explosive corrections
- Enable CCD (Continuous Collision Detection) on fast-moving limbs
- As a last resort for Stage 2: the custom XPBD compute shader solver runs at whatever tick rate we choose (we control the timestep)

### Character Model

**Stage 1: UE5 Manny/Quinn** (built-in mannequin)
- Available via Fab plugin or Third Person template
- Same skeleton as MetaHumans (standard UE5 skeleton)
- Simpler mesh (~15K verts) — ideal for prototyping
- Already has a Physics Asset configured
- Can be swapped for MetaHuman or custom model later without changing any code (same skeleton)

**Stage 2: Optionally upgrade to MetaHuman** for visual fidelity, or keep Manny/Quinn for faster iteration.

---

## 3. Stage 2 Architecture (GPU Migration)

```
Same as Stage 1, except:

  Chaos Physics (CPU)           →  Custom XPBD compute shaders (GPU)
  UPhysicsControlComponent      →  Joint torques applied in compute shader
  Chaos Flesh + ML Deformer     →  Custom XPBD tet-mesh (GPU)
```

Stage 2 replaces CPU-based Chaos systems with custom GPU compute shaders to demonstrate scalability beyond 2 characters.

---

## 4. Technology Stack

| Component | Stage 1 | Stage 2 |
|---|---|---|
| **Joint motor control** | UPhysicsControlComponent | Custom XPBD compute shaders |
| **Skeletal physics** | Chaos Physics (CPU) | Custom XPBD (GPU) |
| **Collision** | Chaos Physics (CPU) | Custom spatial hash + PGS (GPU) |
| **Blend transitions** | UPhysicalAnimationComponent | Custom state logic |
| **Flesh deformation** | Chaos Flesh + ML Deformer | Custom XPBD tet-mesh (GPU) |
| **RL policy inference** | NNE + ONNX Runtime (GPU) | Same |
| **Motion matching** | PoseSearch | Same |
| **Content authoring** | Chaos Flesh Dataflow | Same → export to compute format |
| **Rendering/Camera/Input/Audio/UI** | UE5 built-in | Same |

---

## 5. RL Training (offline, independent of UE5)

### Training Backend

| Backend | Status | Speedup | When to Use |
|---|---|---|---|
| **Isaac Gym** | Stable, proven with PHC | Baseline | Stage 1 — safest option, well-documented |
| **NVIDIA Newton** (via Isaac Lab) | Beta (Sept 2025), open-source, Linux Foundation | 70x for humanoid sims (MuJoCo-Warp) | Stage 2 or if Isaac Gym is too slow — faster training, more accurate physics, differentiable |

Newton is built on NVIDIA Warp + OpenUSD, co-developed with Google DeepMind and Disney Research. ProtoMotions already supports multiple simulator backends (Isaac Gym, Isaac Sim, Isaac Lab), so switching to Newton/Isaac Lab is a config change, not a rewrite. Newton's differentiable physics could also enable gradient-based fine-tuning of the sim-to-sim gap.

### Data Sources

| Source | Format | Use | Notes |
|---|---|---|---|
| **AMASS dataset** | SMPL (native) | Locomotion training for PHC | 40+ hours, 300+ subjects, free for research. No conversion needed |
| **Mixamo** | FBX → AMASS conversion | Fighting-specific clips (punches, kicks, blocks) | Requires retargeting script to SMPL skeleton |

### Training Stages

| Stage | What | Starting Point | Est. Time (4070S) |
|---|---|---|---|
| 1. PHC baseline on AMASS | Locomotion tracking | Existing PHC code + AMASS data | ~2–4 hours |
| 2. Fine-tune on fight mocap | Add Mixamo fighting clips | PHC weights | 4–8 hours |
| 3. Impact response | External force perturbation curriculum | Fine-tuned PHC | 4–8 hours |
| 4. Muscle stiffness head (Stage 2 only) | Extend output for XPBD flesh | Trained policy | 2–4 hours |
| 5. MaskedMimic upgrade (Stage 2 only) | Multi-skill composition from partial cues | Trained PHC as foundation | 8–16 hours |

**Total Stage 1: ~12–24 GPU-hours** (2–4 overnight runs).
**Total with Stage 2 upgrades: ~24–40 GPU-hours** (with Newton, potentially 2–4x faster).

### Sim-to-Sim Transfer

- Stage 1: Isaac Gym → Chaos Physics (rigid-body articulation, similar dynamics)
- The `UPhysicsControlComponent`'s spring/damper drives approximate Isaac Gym's PD controllers. Tuning strength/damping values is the main calibration work.
- If using Newton: Newton's MuJoCo-Warp physics may be closer to Chaos Physics than Isaac Gym's, potentially reducing the sim-to-sim gap.
- **Fallback:** Hand-tuned PD controller via the Physics Control Component with fixed gains. No RL needed — less capable but functional.

### Policy Evolution Path

The RL policy upgrades naturally across stages without architectural changes:

```
Stage 1:  PHC (full-body tracking)
            │  "Track this exact pose from PoseSearch"
            │  Input: full target pose → Output: joint orientations
            │
Stage 2:  MaskedMimic (built on PHC)
            │  "Left hand here, head facing there, figure out the rest"
            │  Input: partial targets + text style → Output: joint orientations
            │  Enables: combat combos from sparse keyframes, text-driven style
            │           ("aggressive", "cautious"), object interaction
```

Both policies output the same format (joint orientations + stiffness), so the UE5 plugin doesn't change — only the ONNX model file is swapped. MaskedMimic is available in ProtoMotions v2.3+ with a pre-trained SMPL model.

**License note:** MaskedMimic's paper is CC BY-NC-SA 4.0 (non-commercial). For commercial use, PHC (check repo license) or a custom-trained policy would be needed.

---

## 6. Content Pipeline

| Asset | Tool | Custom Work? |
|---|---|---|
| Character mesh | Blender → FBX → UE5 | ❌ Standard import |
| Physics Asset (joints, limits, masses) | UE5 Physics Asset editor | ❌ In-editor setup |
| Physics Control Component config | Per-bone strength/damping in editor | ❌ In-editor setup |
| Physical Animation profiles | UE5 Physics Asset editor | ❌ In-editor setup |
| Chaos Flesh (muscles, tet-mesh) | Chaos Flesh Dataflow in-editor | ❌ In-editor setup |
| ML Deformer | Train on Chaos Flesh data in-editor | ❌ In-editor workflow |
| Mocap (locomotion) | AMASS dataset → PoseSearch | ❌ Direct import |
| Mocap (fighting) | Mixamo FBX → UE5 PoseSearch | ❌ Standard import |
| Mocap (training) | AMASS (locomotion) + Mixamo→AMASS (fighting) | ⚠️ Retargeting script |
| PHC policy | ProtoMotions → ONNX → NNE asset | ⚠️ Manual export |

---

## 7. Development Phases

### Stage 1: Prove Animation Quality

#### Phase 0: Feasibility (Weeks 1–2)

**Goal:** Validate that PHC + Chaos Physics produces non-robotic motion before committing.

| Task | Deliverable |
|---|---|
| Set up ProtoMotions + Isaac Gym; run PHC on AMASS locomotion data | Visual: physics-driven motion looks alive |
| Fine-tune PHC on one Mixamo fighting clip | Visual: PHC tracks a punch/kick while balanced |
| Create UE5 5.5 project; Manny with Physics Asset + Physics Control Component | Articulated ragdoll with motor-driven joints |
| Test: drive Physics Control Component targets from C++ each frame | Confirm programmatic joint control works |
| Test: NNE + ONNX Runtime with dummy model | Confirm inference runs in UE5 |
| Test: physics substep stability at 120 Hz and 240 Hz with substepping enabled | Confirm articulated body is stable under PD control |
| Prototype: SMPL ↔ UE5 joint mapping (static table, test with hardcoded poses) | Confirm retargeting produces correct poses |

**Gate G1:** PHC output looks alive in Isaac Gym AND Physics Control Component responds to programmatic targets AND substep rate is stable. If any fails, stop.

#### Phase 1: One Physics-Driven Character (Weeks 3–4)

| Task | Deliverable |
|---|---|
| Write `PhysAnimPlugin`: PoseSearch → NNE inference → Physics Control Component targets | Core plugin (~200 lines) |
| Export PHC policy to ONNX → import as NNE asset | Policy runs in UE5 |
| Set up PoseSearch with Mixamo locomotion + fight clips | Motion matching provides target poses |
| Tune Physics Control Component strength/damping to approximate Isaac Gym dynamics | Reduce sim-to-sim gap |
| Set up Chaos Flesh + ML Deformer on character (optional visual bonus) | Flesh deformation |
| **Gate G2:** Side-by-side — physics-driven vs kinematic PoseSearch | Must look noticeably more natural |

#### Phase 2: Two Characters + Demo (Weeks 5–6)

| Task | Deliverable |
|---|---|
| Second character with same pipeline | Two physics-driven fighters |
| Train impact response policy (Stage 3 training) | Characters react physically to hits |
| Configure Physical Animation Component for knockdown/recovery transitions | Smooth blend between states |
| Input handling (Enhanced Input) for Player 1 | Controllable character |
| Basic AI opponent (state machine or trained policy) | Player 2 fights back |
| Camera (Spring Arm orbital), basic HUD | Fight-game presentation |
| **Gate G3:** Show to observers — "Does this look robotic?" | Subjective but critical |

**Stage 1 deliverable:** Working UE5 demo, two physics-driven fighters, ~200 lines of custom C++.

---

### Stage 2: GPU Migration + Policy Upgrade (conditional — only if Stage 1 succeeds)

#### Phase 3: GPU Solver Foundation + MaskedMimic (Weeks 7–9)

| Task | Deliverable |
|---|---|
| Export tet-mesh + muscle data from Chaos Flesh Dataflow → binary format | GPU-ready asset |
| Implement XPBD articulation solver in HLSL compute shaders | GPU-driven skeleton |
| Implement `UPhysAnimMeshComponent` (compute output → UE5 renderer) | Custom mesh component |
| Benchmark: GPU XPBD vs Chaos CPU for same character | Measured ms comparison |
| Replace Chaos Physics with GPU solver for one character | GPU-driven character matches CPU quality |
| Train MaskedMimic policy in ProtoMotions (on Newton if available) | Multi-skill policy ONNX |
| Swap PHC ONNX → MaskedMimic ONNX in plugin (no code change) | Richer motion repertoire |

#### Phase 4: GPU Collision + Scalability (Weeks 10–12)

| Task | Deliverable |
|---|---|
| Spatial hash + PGS contact solver in compute shaders | GPU collision |
| Two characters on GPU solver | Both fighters GPU-driven |
| Add muscle stiffness output (Stage 4+5 training) | GPU flesh deformation |
| Surface skinning compute shader (tet → surface mesh) | Volumetric flesh on GPU |
| Scalability test: 10–20 GPU-driven characters | Demonstrate GPU advantage |
| Test MaskedMimic partial-target control (sparse keyframes for combos) | Designer-friendly combat authoring |

#### Phase 5: Polish + Demo (Week 13)

| Task | Deliverable |
|---|---|
| Performance profiling + optimization | Stable 60 FPS |
| Side-by-side benchmark: CPU vs GPU at 2/10/20 characters | Quantified GPU advantage |
| Audio polish, final packaging | Distributable `.exe` |
| **Gate G4:** GPU solver matches CPU quality AND scales better | Thesis proven |

---

## 8. Performance Budget

### Stage 1 (CPU physics — comfortable headroom)

| System | Budget | Where |
|---|---|---|
| UE5 rendering | ~8.0 ms | GPU |
| Chaos Physics (2 articulated bodies) | ~1.0 ms | CPU |
| PHC inference (2 chars via NNE) | ~0.4 ms | GPU |
| Physics Control Component | ~0.1 ms | CPU |
| Chaos Flesh + ML Deformer | ~0.5 ms | CPU + GPU |
| PoseSearch + game logic | ~0.5 ms | CPU |
| **Total** | **~10.5 ms** | Well under 16.67 ms |

### Stage 2 (GPU physics — full GPU utilization)

| System | Budget | Where |
|---|---|---|
| UE5 rendering | ~8.0 ms | GPU |
| XPBD compute shaders | ~4.0 ms | GPU |
| PHC inference (NNE) | ~0.4 ms | GPU |
| Collision + contact (compute) | ~1.5 ms | GPU |
| Surface skinning (compute) | ~0.3 ms | GPU |
| PoseSearch + game logic | ~0.5 ms | CPU (parallel) |
| **Headroom** | ~1.97 ms | |
| **Total** | **~16.67 ms** | 60 FPS, GPU-bound |

---

## 9. Risk Register

| Risk | Stage | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| Physics-driven motion doesn't look better | 1 | Medium | **Fatal** | Gate G1 tests in Isaac Gym at Week 2 |
| Sim-to-sim gap (Isaac Gym → Chaos Physics) | 1 | High | High | Physics Control Component's spring/damper drives approximate Isaac Gym's PD controllers. Tune strength/damping |
| Physics Control Component (experimental) is too limited or buggy | 1 | Medium | Medium | Fallback: use raw `AddTorqueInRadians()` on Physics Asset bones (~100 more lines) |
| Chaos Physics substeps insufficient for stability | 1 | Medium | High | Try sync substepping first (120–240 Hz), then async fixed timestep. Increase solver iterations. Ultimate fallback: Stage 2's custom XPBD runs at any rate |
| SMPL ↔ UE5 skeleton mapping produces wrong rotations | 1 | Medium | Medium | Phase 0 tests with hardcoded poses. Coordinate system conversion (Y-up → Z-up) is well-documented |
| NNE/ONNX Runtime too slow or incompatible | 1 | Low | Medium | Model is tiny (~1M params). Fallback: direct TensorRT C++ API |
| PHC doesn't track fighting motions | 1 | Medium | High | Gate G1 tests one clip. Fallback: locomotion-only from AMASS |
| Chaos Flesh too experimental | 1 | Medium | Low | It's a visual bonus; skip it in Stage 1 if problematic |
| GPU XPBD doesn't match CPU Chaos quality | 2 | Medium | Medium | Stage 1 provides ground truth for comparison |
| UE5 RDG compute dispatch complexity | 2 | Medium | Medium | Bypass RDG with raw compute if needed |

---

## 10. Decision Gates

| Gate | When | Question | If No |
|---|---|---|---|
| **G1** | Week 2 | Does PHC look alive? Does Physics Control Component work? | Stop. Total loss: 2 weeks |
| **G2** | Week 4 | Does physics-driven motion look noticeably better than kinematic? | Stop. Thesis disproven |
| **G3** | Week 6 | Is the demo compelling enough to justify GPU work? | Ship Stage 1 as-is |
| **G4** | Week 9 | Does GPU XPBD match CPU solver quality? | Keep Stage 1 CPU version |

---

## 11. Dependencies

| Dependency | Purpose | License | Stage |
|---|---|---|---|
| **Unreal Engine 5.5+** | Rendering, Chaos Physics, PoseSearch, NNE, Physics Control Component, Physical Animation Component, Chaos Flesh, ML Deformer | Epic EULA | 1+2 |
| **ProtoMotions** | RL training framework | Apache 2.0 ✅ | 1+2 |
| **PHC** | Motion tracking policy (Stage 1) | Check repo | 1 |
| **MaskedMimic** | Multi-skill composition policy (Stage 2) | CC BY-NC-SA 4.0 (non-commercial) | 2 |
| **Isaac Gym / Lab** | Training simulation (Stage 1 default) | Free for research | 1 |
| **NVIDIA Newton** | Training simulation (Stage 2 option, 70x faster) | Open source, Linux Foundation | 2 |
| **AMASS dataset** | Locomotion mocap (SMPL format) | Free for research | 1+2 |
| **Mixamo** | Fighting mocap (FBX) | Free | 1+2 |

> No TensorRT. No fTetWild. No Vulkan SDK. No custom Python pipeline. No custom physics math in Stage 1.

---

## 12. Project Structure

```
NewEngine/
├── DESIGN.md
├── RESEARCH.md
├── ENGINEERING_PLAN.md
│
├── Training/                              # Offline RL training
│   ├── ProtoMotions/                      # Cloned repo
│   ├── configs/                           # PHC fine-tuning configs
│   ├── data/
│   │   ├── amass/                         # AMASS locomotion data (SMPL)
│   │   └── mixamo_fight/                  # Mixamo fighting clips (AMASS-converted)
│   ├── scripts/
│   │   └── mixamo_to_amass.py             # Retargeting script
│   └── output/                            # Trained ONNX models
│
└── PhysAnimUE5/                           # UE5 project
    ├── PhysAnimUE5.uproject
    ├── Content/
    │   ├── Characters/                    # FBX skeletal meshes
    │   ├── PhysicsAssets/                 # Articulated ragdoll configs
    │   ├── ChaosFlesh/                    # Flesh simulation assets
    │   ├── PoseSearch/                    # Motion matching databases
    │   ├── MLDeformer/                    # Trained deformer assets
    │   ├── NNEModels/                     # PHC ONNX policies
    │   └── Maps/                          # Arena level
    └── Plugins/
        └── PhysAnimPlugin/
            ├── PhysAnimPlugin.uplugin
            └── Source/
                │  --- Stage 1 (the only custom code) ---
                ├── PhysAnimComponent.h/cpp      # NNE inference + Physics Control bridge
                │
                │  --- Stage 2 additions ---
                ├── PhysAnimMeshComponent.h/cpp   # Custom GPU mesh component
                ├── XPBDSolver.h/cpp              # Compute shader dispatch
                ├── CollisionSystem.h/cpp         # Spatial hash + PGS
                └── Shaders/
                    ├── XPBDSolve.usf
                    ├── SpatialHash.usf
                    ├── ContactSolve.usf
                    └── SurfaceSkin.usf
```

# AGENTS.md — Project Context for AI Agents

> **Read this before doing anything in this codebase.**

## What This Project Is

A real-time physics-driven animation engine built as a UE5 plugin. Instead of playing pre-recorded animations, characters are driven by a neural network (PHC) that applies joint torques through UE5's physics engine, creating motion with real weight, momentum, and balance.

**Scope:** Two human fighters in an arena. Proof of concept, not a shipping game.

## Architecture (LOCKED — Do Not Change)

```
PoseSearch → PHC Policy (NNE/ONNX) → Physics Control Component → Chaos Physics → Renderer
   CPU              GPU                    CPU (built-in)           CPU (built-in)    GPU
```

**Stage 1** (current): All UE5 built-in systems. ~250 lines of custom C++.
**Stage 2** (future): Replace CPU physics with GPU compute shader XPBD.

Read `ENGINEERING_PLAN.md` for full details — it is the source of truth.

## Hard Rules

1. **Do NOT build what UE5 already provides.** Chaos Physics handles collision. Physics Control Component handles PD control. PoseSearch handles motion matching. NNE handles inference. If UE5 has it, use it.
2. **TDD is mandatory.** Write tests before writing implementation code. Every feature must have automated tests. Run `/run-tests` before committing.
3. **No TensorRT dependency.** Use UE5 NNE with ONNX Runtime instead.
4. **No custom Python content pipeline.** Use Chaos Flesh Dataflow for tet-mesh/muscle authoring in the UE5 editor.
5. **PHC is trained offline in ProtoMotions/Isaac Gym.** Training is completely separate from UE5. The only bridge is the `.onnx` file.
6. **SMPL ↔ UE5 skeleton retargeting** is required. PHC uses SMPL (24 joints, Y-up). UE5 uses mannequin skeleton (~70 bones, Z-up). See `.agents/skills/smpl-skeleton/SKILL.md`.
7. **Character model is Manny/Quinn** (UE5 built-in mannequin). Same skeleton as MetaHuman — can upgrade later without code changes.
8. **Commit often.** Small, atomic commits with descriptive messages.

## Directory Structure

```
NewEngine/
├── DESIGN.md                    # Original design vision (do not modify)
├── RESEARCH.md                  # Background research
├── ENGINEERING_PLAN.md          # Source of truth for architecture and phases
├── AGENTS.md                    # ← You are here
│
├── .agents/
│   ├── workflows/               # Step-by-step procedures (use /slash-commands)
│   │   ├── run-tests.md         # /run-tests — run all automated tests
│   │   ├── build-plugin.md      # /build-plugin — compile UE5 plugin
│   │   ├── train-phc.md         # /train-phc — run PHC training
│   │   └── export-onnx.md       # /export-onnx — export model to ONNX
│   └── skills/
│       └── smpl-skeleton/       # SMPL joint definitions and mapping tables
│
├── Training/                    # Python — offline RL training
│   ├── ProtoMotions/            # Cloned repo (gitignored)
│   ├── physanim/                # Our Python package (retargeting, export scripts)
│   ├── tests/                   # pytest test suites
│   ├── configs/                 # Training configs
│   ├── scripts/                 # Utility scripts
│   ├── data/                    # Mocap data (AMASS, Mixamo — gitignored)
│   └── output/                  # Trained models, checkpoints (gitignored)
│
└── PhysAnimUE5/                 # UE5 project (not yet created)
    └── Plugins/
        └── PhysAnimPlugin/      # The custom plugin (~250 lines for Stage 1)
```

## Current Status

**Phase:** Pre-Phase 0 (environment setup)
**What's done:** Engineering plan finalized (v8), TDD test suites written (not yet passing), project scaffolded.
**What's next:** Set up Python environment, clone ProtoMotions, begin Phase 0 feasibility validation.

Check the task list for granular progress: see the artifact `task.md` in the brain directory for this conversation.

## Key Technology Decisions

| Component | Decision | Why |
|---|---|---|
| Physics solver (Stage 1) | Chaos Physics (CPU) | Built-in, proven, good enough for 2 characters |
| Joint motor control | UPhysicsControlComponent | Built-in spring/damper PD controller |
| RL policy | PHC → MaskedMimic (Stage 2) | PHC for full-body tracking, MaskedMimic for partial cues |
| Policy inference | UE5 NNE + ONNX Runtime | Built-in, no TensorRT dependency |
| Motion matching | UE5 PoseSearch | Built-in |
| Training framework | ProtoMotions (Apache 2.0) | NVIDIA's framework, supports PHC + MaskedMimic |
| Training backend | Isaac Gym (Stage 1), NVIDIA Newton (Stage 2 option) | Newton is 70x faster but beta |
| Training data | AMASS (locomotion) + Mixamo (fighting) | AMASS is native SMPL format |
| Character model | Manny/Quinn | Built-in, same skeleton as MetaHuman |

## How to Verify Your Work

1. Run `/run-tests` to execute all automated tests
2. Run `/build-plugin` to compile the UE5 plugin (once UE5 project exists)
3. For visual verification, create an ELI5 instruction for the human user

# AGENTS.md

This is a WINDOWS MACHINE. Do NOT run linux commands like grep, findstr, etc. Use PowerShell syntax instead.

## Project

PHC-Bridge is a UE5 proof-of-concept bridge between an offline-trained PHC-family policy and Unreal Engine runtime systems.

Primary goal:
- drive a physics-based humanoid in UE5 from a neural policy

Secondary goals:
- keep the UE bridge small
- maximize reuse of UE5 built-ins
- preserve a clean separation between offline training and runtime inference

## Architecture Lock

Do not change this architecture unless explicitly asked for an architecture review.

PoseSearch -> PHC Policy (NNE/ONNX) -> Physics Control Component -> Chaos Physics -> Renderer

Interpretation:
- motion selection/search belongs to PoseSearch
- policy inference belongs to UE5 NNE with ONNX Runtime
- low-level actuation belongs to Physics Control
- simulation belongs to Chaos
- training belongs outside UE5

## Hard Rules

1. Prefer UE5 built-ins over custom systems.
2. Keep training and runtime separate.
3. No TensorRT dependency.
4. No custom Python pipeline for UE5 asset authoring.
5. Use TDD by default for all deterministic logic. TDD is optional only for: live runtime/editor/physics behavior, visual/manual quality checks, short exploratory spikes.
6. Any exploratory spike must convert its deterministic logic into tests before the work is considered complete.
7. Do not leave permanent fail-by-design or permanent skip-by-design tests in the main suite without an explicit temporary reason and removal plan.
9. Treat Manny/Quinn as the default runtime skeleton unless changed explicitly.
9. Keep commits small and atomic.
10. Build with .\scripts\build.ps1
11. If you ran any smoke tests, then read the logs with "python .\scripts\read_logs.py". If you didn't, then ignore this step.

## Anti-Spiral Rule

Do not debug balance visually.

A visual improvement is not progress unless it is explained by:
- a mapped test
- a canonical terminal reason
- populated artifact fields
- one explicit hypothesis
- one owning code surface

If an in-engine failure cannot be explained by artifacts, stop implementation and improve instrumentation or contracts before tuning behavior.

## Response Style

When working in this repo:
- make file edits directly instead of pasting code into chat
- do not include large code snippets or diffs unless explicitly requested
- after edits, reply with:
  - one-sentence summary of all changes
- keep responses short

## What To Read

Use `AGENTS.md` for project rules.

## Constraint To Remember

The bridge is supposed to stay small.

If a proposal replaces an existing UE5 subsystem with a large custom runtime system, it is probably the wrong move.

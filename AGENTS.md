# AGENTS.md

This is a Windows machine. Use PowerShell commands. Do not use Linux shell commands.

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

`PoseSearch -> PHC Policy (NNE/ONNX) -> Physics Control Component -> Chaos Physics -> Renderer`

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
5. Use TDD for deterministic logic.
6. Exploratory spikes must become deterministic tests before the work is complete.
7. Do not leave permanent fail-by-design or skip-by-design tests in the main suite.
8. Treat Manny/Quinn as the default runtime skeleton unless explicitly changed.
9. Keep commits small and atomic.
10. Build with `.\\scripts\\build.ps1`.
11. If smoke tests were run, read logs with `python .\\scripts\\read_logs.py`.

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
5. Use TDD when practical.
6. Treat Manny/Quinn as the default runtime skeleton unless changed explicitly.
7. Keep commits small and atomic.
8. Build with .\scripts\build.ps1
9. If you ran any smoke tests, then read the logs with "python .\scripts\read_logs.py". If you didn't, then ignore this step.
10. If you don't know where some functionality is located in the code, then use the mcp graph. If you search for something and do NOT find it, then map it so that future interactions don't have to.

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

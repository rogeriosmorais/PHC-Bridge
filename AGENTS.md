# AGENTS.md

Read this first.

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

## Response Style

When working in this repo:
- make file edits directly instead of pasting code into chat
- do not include large code snippets or diffs unless explicitly requested
- after edits, reply with:
  - files changed
  - one-sentence summary of each change
  - exact verification command
- keep responses short

## What To Read

Use `AGENTS.md` for project rules.
Use `.agents/skills/*` for specialized instructions and reference material.

## Constraint To Remember

The bridge is supposed to stay small.

If a proposal replaces an existing UE5 subsystem with a large custom runtime system, it is probably the wrong move.

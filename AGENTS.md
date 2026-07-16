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
10. Build with `.\scripts\build.ps1`. Run UE tests through the same script with `-Test "Test Name"`.
11. If smoke tests were run, read logs with `python .\scripts\read_logs.py`.
12. Do not repeat the same failing test more than 3 times. After the third unchanged failure, stop and change direction.
13. Never use raw `UE_LOG` in Unreal Engine C++ code. Use `PHYSANIM_LOG_RATE_LIMITED` from `PhysAnimLogger.h`. Un-rate-limited logging is forbidden in loops, ticks, or other high-frequency paths. Lifecycle and unrecoverable failure logs may be un-rate-limited; temporary debugging logs must be removed before commit.
14. Local tests, diagnostic classifiers, evidence summaries, graph nodes, completed workflow states, and harness scores never constitute product success.
15. Product behavior is judged only by a versioned product protocol executed against raw UE runtime observations and its required negative controls.
16. Locked product protocols are append-only. Protocol changes and runtime changes must be separate commits. A failing runtime may not be made green by editing the active protocol.
17. Product tests must distinguish `PASS`, behavioral `FAIL`, malformed-run `INVALID`, and missing-environment `BLOCKED`.
18. Positive product paths may not restart the bridge, force completion, relax thresholds, unlock control groups, enable hidden shell assistance, or accept safe denial as success.
19. Same-machine execution is acceptable. Git history, immutable protocol versions, raw artifacts, and discriminative negative controls are the integrity boundary.
20. Treat `F:\NewEngine-AgentB` as the entire project scope. Do not read from or depend on sibling `F:\NewEngine` checkouts.

## Test Tiers

- `npm run test:fast` covers deterministic unit and contract logic. Synthetic fixtures in this tier prove evaluator or serializer behavior only.
- `npm run test:runtime` covers a real UE runtime integration attempt.
- `npm run test:product` covers the complete causal product protocol and negative controls.
- `npm test` runs every tier. Only `test:product` can produce a product verdict.

## Knowledge Navigation

This project has a knowledge graph under `graphify-out/`.

- For codebase questions, run `graphify query "<question>"` first when `graphify-out/graph.json` exists.
- Use `graphify path` and `graphify explain` for relationships and focused concepts.
- Prefer `graphify-out/wiki/index.md` for broad navigation.
- After modifying code, run `graphify update .`.

## Serena-First Code Intelligence

Serena is the required first-line tool for code navigation and symbol-aware work.

1. Before any code task, call Serena's `initial_instructions` and ensure `PHC-Bridge` is the active project.
2. Use Serena's `get_symbols_overview` or `find_symbol` before reading a C++ source or header file in full.
3. Use `find_referencing_symbols`, `find_declaration`, or `find_implementations` before changing a symbol or its contract.
4. Prefer Serena's symbol-aware editing tools for whole-symbol changes. Use normal patch editing for small changes within a symbol.
5. After changing C++ code, run Serena diagnostics on the affected files before building or testing.
6. Do not launch concurrent Serena code-intelligence requests while clangd is performing its initial Unreal Engine index. Issue requests sequentially until the index is warm.
7. Delegated agents and subagents must follow these Serena requirements independently; the parent agent must include this requirement in delegated task instructions.
8. Graphify remains the required first step for broad codebase, architecture, and relationship questions. Use Serena after Graphify for live symbol-level verification.
9. Fall back to `rg`, targeted file reads, or other local tools only when Serena is unavailable, times out, lacks the required operation, or returns unusable results. State the fallback reason explicitly and do not represent fallback results as Serena-derived evidence.
10. Serena availability, local indexing, symbol results, or diagnostics are development evidence only and do not constitute product success.

## Current Product Direction

The active milestone is causal standing with Manny:

- real PoseSearch idle selection
- real NNE inference from the imported ONNX in this checkout
- real Physics Control target application and readback
- fully simulated Chaos bodies without Character Movement or capsule assistance
- measurable recovery from a fixed pelvis perturbation
- material improvement over zero-action and dropped-dispatch controls

Walking and Quinn validation follow only after causal standing passes.

## Mandatory Path Forward

All implementation work for the current milestone must follow:

`plans/stage1/20-execution/mandatory-path-forward.md`

If that document conflicts with older execution notes, follow the mandatory path forward document for the current causal-standing work until it is explicitly superseded.

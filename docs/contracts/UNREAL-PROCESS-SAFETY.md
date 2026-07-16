# Unreal Process Safety Contract

## Scope

This repository targets Unreal Engine 5.7. Project scripts must never terminate an Unreal Engine 5.8 process or any Unreal process whose executable cannot be proven to belong to the configured UE 5.7 engine root.

## Locked invariants

1. `UE5_PATH` must resolve to an engine whose `Build/Build.version` reports major version `5` and minor version `7`.
2. The build script does not terminate processes by default.
3. Process cleanup is explicit through `-CloseEngineProcesses`.
4. Even with that switch, a process is eligible only when its executable path is inside the validated UE 5.7 `UE5_PATH` root.
5. A process with an unavailable, empty, or inaccessible executable path is never terminated.
6. Unreal processes from UE 5.8, another UE installation, or an unrelated tool directory are ignored.
7. No other project script may contain a direct process-termination command. The repository test suite enforces this rule.
8. Automation timeout cleanup must validate the entire captured process tree before terminating anything. Any UE process in that tree must have an executable path inside the configured UE 5.7 engine root; otherwise cleanup aborts without terminating the tree.

## Build behavior

When a UE 5.7 process owned by the configured engine is running:

- the normal build exits with `BLOCKED` and does not terminate it;
- the operator may close it manually; or
- the operator may explicitly use `-CloseEngineProcesses`, which remains restricted to the validated UE 5.7 engine root.

A running UE 5.8 editor does not block the build and is never selected for cleanup.

## Automation orchestration behavior

`run_ue_automation_episode.ps1` applies the same engine ownership rules:

- duplicate-process checks consider only Unreal processes owned by the configured UE 5.7 root;
- a UE 5.8 editor does not block an episode dry run or execution;
- timeout cleanup enumerates the child process tree launched by the orchestrator;
- if any Unreal descendant belongs to UE 5.8, another engine root, or has no verifiable path, cleanup fails closed before terminating any member of that tree;
- validated trees are terminated deepest-child-first so the wrapper and UE 5.7 descendants cannot remain orphaned.

## Enforcement

The implementation is centralized in:

- `scripts/UnrealProcessSafety.psm1`
- `scripts/build.ps1`
- `scripts/run_ue_automation_episode.ps1`

The regression tests are in:

- `scripts/tests/test_unreal_process_safety.py`

The tests verify engine-version rejection, path ownership, inaccessible-path fail-closed behavior, build-script behavior, orchestration timeout-tree validation, UE 5.8 descendant rejection, and the absence of unscoped process-kill commands elsewhere in the project.

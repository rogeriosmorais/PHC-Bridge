---
name: graph-plan
description: Execute the PLAN phase of mcp-graph lifecycle — smart decompose, sprint planning, DORA-based estimation, cross-project learning
triggers:
  - graph-plan
version: 2.0.0
author: Diego Nogueira
date: 2026-04-04
---

# graph-plan

Execute the PLAN phase of the mcp-graph lifecycle. Decomposes epics into atomic tasks (auto or manual), plans sprints, maps dependencies, and prepares for implementation using DORA metrics for estimation.

## When to Use

- After DESIGN phase is complete (ADRs documented, architecture defined)
- Breaking epics into implementable tasks
- Planning sprint scope and priorities
- The `_lifecycle.phase` returned by mcp-graph is `PLAN`

## Mandatory Flow

```
context → learn_from_project → smart_decompose / node(add) → edge → plan_sprint → sync_stack_docs → forecast(dora) → analyze(sprint_health) → set_phase(IMPLEMENT)
```

## Workflow

### Step 1: Load Context

```
Tool: mcp__mcp-graph__context (id: <epic node>)
Tool: mcp__mcp-graph__context(action: "rag") (query: <epic title + decisions>, detail: "deep")
```

Review ADRs, requirements, and architecture from DESIGN phase.

### Step 2: Import Cross-Project Estimates (optional)

```
Tool: mcp__mcp-graph__learn_from_project
Params: sourcePath: "<path>/workflow-graph/graph.db", categories: ["estimates", "patterns"]
```

Use historical velocity data to improve estimation accuracy.

### Step 3: Decompose Epics

**Option A — Smart Decompose (v6.0 recommended):**
```
Tool: mcp__mcp-graph__analyze (mode: "smart_decompose", nodeId: <epic_id>)
```

Auto-creates subtasks: 1 AC = 1 subtask with test type inference:

| Keywords in AC | Inferred test type |
|----------------|-------------------|
| api, endpoint, database, persists, sync, fetch, http | **integration** |
| page, click, browser, redirect, ui, form, button | **e2e** |
| Everything else | **unit** |

**Option B — Manual decomposition:**
```
Tool: mcp__mcp-graph__node (action: "add", type: "task")
Tool: mcp__mcp-graph__node (action: "add", type: "subtask")
```

**Atomic decomposition rules (XP Anti-Vibe-Coding):**
- Each task completable in ≤ 2h
- Each task has clear AC with testable assertions
- Each task has XP size estimate (XS, S, M, L, XL)
- Prefer many small tasks over few large ones

### Step 4: Map Dependencies

```
Tool: mcp__mcp-graph__edge
```

Edge types: `task → task`, `subtask → task`, `task → epic`, `task → decision`.

### Step 5: Plan Sprint

```
Tool: mcp__mcp-graph__plan_sprint
```

Assign tasks based on: priority, dependencies, size, risk (tackle risky items early).

### Step 6: Sync Stack Documentation

```
Tool: mcp__mcp-graph__sync_stack_docs
```

Ensures knowledge base has current API docs for the stack. **Mandatory before IMPLEMENT.**

### Step 7: Review DORA Metrics

```
Tool: mcp__mcp-graph__forecast (mode: "dora")
```

Use historical velocity (deployment frequency, lead time) to calibrate sprint capacity.

### Step 8: Validate Sprint Health

```
Tool: mcp__mcp-graph__analyze (mode: "sprint_health")
```

Score the sprint plan: balanced load, no oversized tasks, dependencies resolved.

### Step 9: Validate Readiness

```
Tool: mcp__mcp-graph__analyze (mode: "ready")
```

**Gate criteria:**
- All epics have task decomposition
- Tasks have acceptance criteria
- Dependencies mapped (no cycles)
- Sprint assignments exist
- No oversized tasks (> L without subtask decomposition)

### Step 10: Transition

Once gate passes:
```
Tool: mcp__mcp-graph__set_phase (phase: "IMPLEMENT")
```

Follow `_lifecycle.nextAction` for the recommended next tool call.

## Output Format

```
Phase: PLAN → IMPLEMENT
Tasks: N tasks, M subtasks created (K via smart_decompose)
Sprints: J sprints planned
Dependencies: D edges mapped
DORA: velocity X tasks/day, lead time P85 Yh
Gate: ready — score N/100, grade X
Status: Ready to proceed to IMPLEMENT phase
```

## Anti-Patterns

- Do NOT write code during PLAN — this phase is planning only
- Do NOT create tasks larger than 2h — use `smart_decompose` or decompose manually
- Do NOT skip acceptance criteria — they drive TDD in IMPLEMENT
- Do NOT ignore dependencies — they determine execution order
- Do NOT plan everything at once — plan 1-2 sprints ahead, refine later
- Do NOT ignore `_lifecycle.nextAction` — it guides the optimal next action
- Do NOT use deprecated tool names (`add_node`) — use `node (action: "add")`
- Do NOT skip `sync_stack_docs` — it is a mandatory prerequisite for IMPLEMENT


## Codex Notes

- In Codex Plan Mode, use this skill for planning only and do not mutate files.
- During implementation, follow the project `AGENTS.md` rules and use `apply_patch` for manual edits.

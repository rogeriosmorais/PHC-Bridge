---
name: graph-analyze
description: Execute the ANALYZE phase of mcp-graph lifecycle — PRD creation, requirements, Definition of Ready (7 checks), cross-project learning
triggers:
  - graph-analyze
version: 2.0.0
author: Diego Nogueira
date: 2026-04-04
---

# graph-analyze

Execute the ANALYZE phase of the mcp-graph lifecycle. Creates the PRD, defines requirements, and imports them into the execution graph. Entry point of the 9-phase lifecycle.

## When to Use

- Starting a new project or feature from scratch
- Defining requirements for a new epic
- Importing an existing PRD into the graph
- The `_lifecycle.phase` returned by mcp-graph is `ANALYZE`

## Mandatory Flow

```
[learn_from_project] → import_prd / node(add) → edge → analyze(prd_quality) → analyze(ready) → set_phase(DESIGN)
```

## Workflow

### Step 0: Bootstrap Knowledge (optional)

If similar projects exist, import knowledge to accelerate analysis:
```
Tool: mcp__mcp-graph__learn_from_project
Params: sourcePath: "<path>/workflow-graph/graph.db", categories: ["errors", "estimates", "adrs", "patterns"]
```

Check current knowledge gaps:
```
Tool: mcp__mcp-graph__knowledge_stats
```

### Step 1: Understand the Scope

Ask the user what they want to build. Gather:
- Problem statement
- Target users
- Core features (MVP scope)
- Non-functional requirements
- Known constraints

### Step 2: Create or Import PRD

**Option A — Import existing PRD:**
```
Tool: mcp__mcp-graph__import_prd
```

**Option B — Create PRD from conversation:**
Write a structured PRD covering:
1. Vision and overview
2. Problem definition
3. Product objectives
4. Architecture overview
5. Functional requirements
6. Non-functional requirements
7. Risk analysis

Save as `prd.md` in the project root, then import.

### Step 3: Structure Requirements as Nodes

For each major section, create graph nodes:
```
Tool: mcp__mcp-graph__node (action: "add", type: "requirement")
Tool: mcp__mcp-graph__node (action: "add", type: "epic")
```

### Step 4: Create Edges

Link requirements to epics, epics to milestones:
```
Tool: mcp__mcp-graph__edge
```

Edge types: `requirement → epic`, `epic → milestone`, `requirement → requirement`.

### Step 5: Risk & Constraint Analysis

Create risk and constraint nodes:
```
Tool: mcp__mcp-graph__node (action: "add", type: "risk")
Tool: mcp__mcp-graph__node (action: "add", type: "constraint")
```

Link risks to the requirements/epics they affect.

### Step 6: Save Key Decisions

```
Tool: mcp__mcp-graph__write_memory (category: "decision")
```

Record requirement decisions that should inform future phases.

### Step 7: Validate — PRD Quality

```
Tool: mcp__mcp-graph__analyze (mode: "prd_quality")
```

### Step 8: Validate — Definition of Ready (7 Checks)

```
Tool: mcp__mcp-graph__analyze (mode: "ready")
```

| # | Check | What it verifies |
|---|-------|-----------------|
| 1 | `has_requirements` | ≥1 epic or requirement in graph |
| 2 | `has_acceptance_criteria` | Tasks or AC nodes exist |
| 3 | `no_orphans` | No orphan requirements or tasks |
| 4 | `no_cycles` | No dependency cycles |
| 5 | `has_constraints` | ≥1 constraint node |
| 6 | `has_risks` | ≥1 risk node |
| 7 | `prd_quality_score` | Score ≥ 60 |

If validation fails, fix identified issues and re-run.

### Step 9: Transition

Once both gates pass:
```
Tool: mcp__mcp-graph__set_phase (phase: "DESIGN", mode: "strict", codeIntelligence: "strict", prerequisites: "strict")
```

Follow `_lifecycle.nextAction` for the recommended next tool call.

## Output Format

```
Phase: ANALYZE → DESIGN
PRD: imported (N requirements, M epics, K risks, J constraints)
Gate: prd_quality — score N/100, grade X
Gate: ready — 7/7 checks passed
Knowledge: L docs imported from cross-project learning
Status: Ready to proceed to DESIGN phase
```

## Anti-Patterns

- Do NOT start coding during ANALYZE — this phase is requirements only
- Do NOT create task-level nodes yet — that happens in PLAN
- Do NOT skip risk/constraint analysis — they inform DESIGN decisions
- Do NOT ignore `_lifecycle.nextAction` — it guides the optimal next action
- Do NOT use deprecated tool names (`add_node`) — use `node (action: "add")`


## Codex Notes

- In Codex Plan Mode, use this skill for planning only and do not mutate files.
- During implementation, follow the project `AGENTS.md` rules and use `apply_patch` for manual edits.

---
name: graph-design
description: Execute the DESIGN phase of mcp-graph lifecycle — ADRs, architecture decisions, contract coverage, Code Intelligence impact analysis
triggers:
  - graph-design
version: 2.0.0
author: Diego Nogueira
date: 2026-04-04
---

# graph-design

Execute the DESIGN phase of the mcp-graph lifecycle. Defines architecture, creates ADRs, establishes technical design, and validates contract coverage before planning.

## When to Use

- After ANALYZE phase is complete (PRD imported, requirements defined)
- Making architectural decisions for a new feature
- Documenting technical choices (ADRs)
- The `_lifecycle.phase` returned by mcp-graph is `DESIGN`

## Mandatory Flow

```
context → context(action: "rag") → node(add decision) → edge → code_intelligence → analyze(adr) → analyze(contract_coverage) → analyze(design_ready) → set_phase(PLAN)
```

## Workflow

### Step 1: Load Context

```
Tool: mcp__mcp-graph__context (id: <epic or requirement node>)
Tool: mcp__mcp-graph__context(action: "rag") (query: <architecture keywords>, detail: "deep")
```

Review requirements, constraints, and risks from ANALYZE phase.

### Step 2: Impact Analysis (for existing codebases)

If modifying existing modules, run Code Intelligence first:
```
Tool: mcp__mcp-graph__code_intelligence (action: "impact", symbol: "<module_name>", depth: 2)
```

Understand what components will be affected by design decisions.

### Step 3: Create ADR Decision Nodes

For each significant architectural choice:
```
Tool: mcp__mcp-graph__node (action: "add", type: "decision")
```

**ADR Format for description:**
```markdown
## Status: Accepted
## Context: [Why this decision is needed]
## Decision: [What was decided and how]
## Consequences: [Trade-offs, follow-up work, risks accepted]
```

**Common decisions:** Technology stack, data storage, API design, communication patterns, error handling, testing strategy, deployment model.

### Step 4: Link Decisions

```
Tool: mcp__mcp-graph__edge
```

Edge types: `decision → requirement`, `decision → epic`, `decision → risk`, `decision → decision`.

### Step 5: Interface Design

Define contracts between components. Document as constraint nodes:
```
Tool: mcp__mcp-graph__node (action: "add", type: "constraint")
```

### Step 6: Save Architectural Decisions

```
Tool: mcp__mcp-graph__write_memory (category: "decision")
```

Record key architectural choices for future RAG retrieval.

### Step 7: Validate ADRs

```
Tool: mcp__mcp-graph__analyze (mode: "adr")
```

Verify ADR completeness and quality.

### Step 8: Validate Contract Coverage

```
Tool: mcp__mcp-graph__analyze (mode: "contract_coverage")
```

Verify interface/contract completeness across components.

### Step 9: Validate Design Gate

```
Tool: mcp__mcp-graph__analyze (mode: "design_ready")
```

**Gate criteria:**
- Key architectural decisions documented as ADR nodes
- Decisions linked to requirements they address
- No orphan requirements without design coverage
- Interface contracts defined

If validation fails, add missing ADRs or edges.

### Step 10: Transition

Once gate passes:
```
Tool: mcp__mcp-graph__set_phase (phase: "PLAN")
```

Follow `_lifecycle.nextAction` for the recommended next tool call.

## Output Format

```
Phase: DESIGN → PLAN
ADRs: N decision nodes created
Contracts: M interface constraints defined
Impact: K existing modules analyzed
Coverage: J/T requirements addressed by decisions
Gate: design_ready — score N/100, grade X
Status: Ready to proceed to PLAN phase
```

## Anti-Patterns

- Do NOT create implementation tasks during DESIGN — that happens in PLAN
- Do NOT write code — design is documentation and decision-making only
- Do NOT skip ADRs — undocumented decisions lead to inconsistency
- Do NOT over-design — focus on decisions that affect MVP scope
- Do NOT ignore `_lifecycle.nextAction` — it guides the optimal next action
- Do NOT use deprecated tool names (`add_node`) — use `node (action: "add")`
- Do NOT skip Code Intelligence on existing codebases — impact analysis prevents surprises


## Codex Notes

- In Codex Plan Mode, use this skill for planning only and do not mutate files.
- During implementation, follow the project `AGENTS.md` rules and use `apply_patch` for manual edits.

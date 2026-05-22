---
name: graph-architecture
description: Architecture governance using C4 Model, ADR lifecycle, Architecture Fitness Functions, layer boundary enforcement, and drift detection
triggers:
  - graph-architecture
version: 1.0.0
author: Diego Nogueira
date: 2026-04-04
---

# graph-architecture

Architecture governance using C4 Model (Context, Container, Component, Code), ADR lifecycle management, Architecture Fitness Functions, layer boundary enforcement, and architecture drift detection. Ensures the system's documented architecture stays aligned with the actual codebase over time.

## When to Use

- During DESIGN phase for new features
- Quarterly architecture reviews
- When coupling analysis shows degradation
- When onboarding new developers to understand system structure
- Before major refactors

## Mandatory Flow

```
C4 context → C4 container → C4 component → ADR inventory → fitness functions → layer boundaries → drift detection → report → write_memory
```

## Workflow

### Step 1: C4 Context Diagram

Map the system's external boundaries. Identify: users (developers, AI agents), external systems (GitHub, Context7, Playwright, SQLite), and the mcp-graph system itself. Generate mermaid diagram via `mcp__mcp-graph__export(action:"mermaid", format:"flowchart")`. Document: who uses the system, what external dependencies exist, what data flows in/out.

### Step 2: C4 Container Diagram

Map internal containers: CLI (Commander.js), MCP Server (tools), REST API (Express), Dashboard (React), SQLite Store, Knowledge Store, Code Intelligence Engine. For each container: technology, responsibility, communication protocols. Verify containers match `src/` directory structure (`cli/`, `mcp/`, `api/`, `web/`, `core/store/`, `core/code/`).

### Step 3: C4 Component Diagram

For each container, map key components. Core modules: `parser/`, `importer/`, `planner/`, `context/`, `rag/`, `search/`, `insights/`, `integrations/`. Verify component boundaries: `core/` never imports from `cli/` or `mcp/` (dependency direction rule from CLAUDE.md). Use `mcp__mcp-graph__code_intelligence` for real dependency analysis.

### Step 4: ADR Inventory & Lifecycle

List all decision nodes in graph via `mcp__mcp-graph__list(type:"decision")`. Verify each ADR has: Status (Proposed/Accepted/Deprecated/Superseded), Context, Decision, Consequences. Check for stale ADRs (decisions no longer relevant). Use `mcp__mcp-graph__analyze(mode:"adr")` for quality scoring.

### Step 5: Architecture Fitness Functions

Define automated checks that verify architecture properties hold:

| Function | Tool | What It Checks |
|----------|------|----------------|
| No circular dependencies | `mcp__mcp-graph__analyze(mode:"cycles")` | Dependency graph is acyclic |
| Layer isolation | grep imports | `core/` doesn't import `mcp/` or `cli/` |
| Coupling score | `mcp__mcp-graph__analyze(mode:"coupling")` | Module coupling within thresholds |
| Interface completeness | `mcp__mcp-graph__analyze(mode:"interfaces")` | Public contracts fully typed |

Score each fitness function pass/fail.

### Step 6: Layer Boundary Enforcement

Verify the dependency direction rule: `schemas/` <- `core/` <- `mcp/` <- `cli/`. Check for violations: grep for imports that cross layer boundaries in the wrong direction. Flag: core importing from mcp, schemas importing from core, cli containing business logic. Cross-reference with CLAUDE.md rules.

### Step 7: Architecture Drift Detection

Compare current codebase structure with documented architecture (C4 diagrams, ADRs). Detect: new modules not in any diagram, deprecated modules still in use, component responsibilities that shifted, new external dependencies not documented. Use `mcp__mcp-graph__analyze(mode:"code_sync")` for reference staleness.

### Step 8: Architecture Report

Score per dimension (C4 completeness, ADR quality, fitness functions, layer compliance, drift). Generate updated C4 diagrams as mermaid. List architectural debt items. Save via `mcp__mcp-graph__write_memory`.

## Output Format

```
Phase: ARCHITECTURE GOVERNANCE
C4 Diagrams: Context (N actors, N systems), Container (N containers), Component (N components)
ADR Inventory: N total (N accepted, N deprecated, N stale)
ADR Quality Score: N/100
Fitness Functions: N/N passed
Layer Violations: N found
Architecture Drift: N items detected
Architectural Debt: N items
Overall Architecture Health: Grade A-F

Saved to memory: "Architecture Review — <date>"
```

## Anti-Patterns

- Do NOT document architecture only once — it drifts
- Do NOT skip C4 Context — it defines system boundaries
- Do NOT create ADRs without Consequences section — trade-offs matter
- Do NOT ignore layer boundary violations — they compound into spaghetti
- Do NOT skip fitness functions — automated checks catch drift early
- Do NOT let ADRs go stale — review quarterly
- Do NOT over-architect — document what exists, not what you wish existed


## Codex Notes

- In Codex Plan Mode, use this skill for planning only and do not mutate files.
- During implementation, follow the project `AGENTS.md` rules and use `apply_patch` for manual edits.

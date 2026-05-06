---
name: graph-refactor
description: Tech debt management and refactoring audit using SQALE method, complexity analysis, dead code detection, and KISS/YAGNI/DRY enforcement
triggers:
  - graph-refactor
version: 1.0.0
author: Diego Nogueira
date: 2026-04-04
---

# graph-refactor

Tech debt management and refactoring audit using SQALE method, complexity analysis, dead code detection, and KISS/YAGNI/DRY enforcement. Identifies code that is too complex, duplicated, unused, or over-engineered, and produces a prioritized refactoring plan tracked in the execution graph.

## When to Use

- During LISTENING phase — track tech debt for future sprints
- Before major features — reduce complexity to make new code easier to add
- When code quality score drops below B — systematic cleanup needed
- At sprint boundaries — allocate 15-20% of capacity for tech debt reduction

## Mandatory Flow

```
complexity scan --> dead code detection --> duplication analysis --> KISS/YAGNI audit --> tech debt scoring --> refactoring plan --> test verification --> report --> write_memory
```

## Workflow

### Step 1: Complexity Scan

Measure cyclomatic complexity per function across the codebase:

Use `mcp__mcp-graph__code_intelligence` for symbol-level analysis to enumerate all functions and methods.

**Complexity thresholds:**

| Complexity | Level | Action |
|-----------|-------|--------|
| 1-5 | Simple | No action |
| 6-10 | Moderate | Monitor |
| 11-20 | Warning | Schedule refactor |
| >20 | Critical | Refactor immediately |

Additionally check:
- **Nesting depth:** Flag functions with >3 levels of nesting (if/for/try stacking)
- **Function length:** Flag functions >50 lines — candidates for Extract Method
- **Parameter count:** Flag functions with >5 parameters — candidates for Parameter Object
- **File length:** Flag files >500 lines — candidates for module splitting

### Step 2: Dead Code Detection

Find code that is never used:

- **Unused exports:** Functions/types exported but never imported elsewhere
- **Unreachable code:** Code after `return`, `throw`, `break`, `continue`
- **Commented-out code:** Blocks of commented code (>3 lines) — should be deleted, not preserved
- **Unused imports:** Imports that are not referenced in the file
- **Unused variables:** Variables declared but never read

Use `mcp__mcp-graph__analyze(mode:"code_sync")` for stale references and orphaned symbols.

Run ESLint for automated detection:
```bash
npx eslint src/ --rule '{"no-unused-vars":"error","no-unreachable":"error"}'
```

### Step 3: Duplication Analysis

Detect copy-paste code patterns:

- Search for code blocks >10 lines that appear in multiple files with minor variations
- Check for functions that do the same thing with different names across modules
- Identify utility patterns that should be extracted into `src/core/utils/`
- Look for repeated validation logic that could use shared Zod schemas
- Check for similar store query patterns that could be abstracted

Common duplication hotspots in this codebase:
- Store initialization patterns across test files
- Error handling patterns across MCP tools
- Zod schema definitions that overlap

### Step 4: KISS/YAGNI Audit

Check for over-engineering:

- **Unnecessary abstractions:** Interfaces with a single implementation, abstract classes with one subclass
- **Premature generalization:** Generic code that handles cases that don't exist yet
- **Unused configuration:** Options, flags, or settings that no caller uses
- **Features without callers:** Exported functions with zero import sites
- **Speculative code:** Code paths protected by feature flags that are always on/off
- **Over-parameterized functions:** Functions that accept options objects with >10 optional fields

For each finding, ask: "Does this complexity serve a current requirement?" If not, flag it.

### Step 5: Tech Debt Scoring (SQALE)

Calculate Technical Debt Ratio using the SQALE method:

```
Technical Debt Ratio = remediation_cost / development_cost
```

Categorize debt by difficulty:

| Category | Difficulty | Examples |
|----------|-----------|----------|
| Architecture | Hard | Module boundaries, circular deps, layer violations |
| Design | Medium | Missing interfaces, tight coupling, wrong abstractions |
| Code | Easy | Long functions, magic numbers, naming, duplication |

Prioritize by **impact x frequency of change** (hotspot analysis):

```bash
git log --format=format: --name-only --since="90 days" -- src/ | sort | uniq -c | sort -rn | head -20
```

Files with high churn AND high debt are the top priority — they cause the most pain.

### Step 6: Refactoring Plan

For each significant finding, propose a specific refactoring technique:

| Pattern | Refactoring | Effort |
|---------|-------------|--------|
| Long function | Extract Method | XS-S |
| Deep nesting | Guard Clauses / Early Return | XS |
| Duplicate code | Extract Shared Utility | S-M |
| Large class/file | Split Module | M |
| Complex conditional | Replace Conditional with Polymorphism | M-L |
| God object | Decompose into focused modules | L |
| Tight coupling | Introduce Interface / Dependency Injection | M-L |

Create graph nodes for significant refactorings (M or larger):
```
Tool: mcp__mcp-graph__node (action: "add", type: "task", tags: ["tech-debt", "<category>"])
```

Include in node description: what to refactor, why, estimated effort, and affected files.

### Step 7: Test Verification

Ensure refactoring safety net exists:

- Verify all refactoring candidates have existing tests — if not, tests must be added BEFORE refactoring
- Run `npm test` to confirm green baseline
- Flag refactoring targets without test coverage as "blocked — needs tests first"
- Check test quality for refactoring targets — shallow tests won't catch regressions

```bash
npm test
```

For each refactoring candidate, note whether tests exist:
- Has tests + good coverage = safe to refactor
- Has tests + shallow coverage = add edge case tests first
- No tests = write tests BEFORE touching the code

### Step 8: Debt Report

Compile the full tech debt report:

**Scoring:**
- **A (90-100):** Low complexity, no dead code, minimal duplication, debt ratio <5%
- **B (75-89):** Some moderate complexity, minor duplication, debt ratio 5-10%
- **C (60-74):** Several complex functions, noticeable duplication, debt ratio 10-20%
- **D (45-59):** High complexity, significant dead code, debt ratio 20-35%
- **F (< 45):** Critical tech debt, pervasive duplication, debt ratio >35%

Save findings:
```
Tool: mcp__mcp-graph__write_memory (title: "Tech Debt Audit — <date>", content: <report>)
```

## Output Format

```
Phase: TECH DEBT AUDIT
Complexity: <N> functions >10 (<N> critical >20), avg complexity: <N>
Dead Code: <N> unused exports, <N> unreachable blocks, <N> commented blocks
Duplication: <N>% estimated duplication, <N> duplicate patterns found
KISS/YAGNI: <N> over-engineered patterns detected
SQALE Debt Ratio: <N>% (architecture: <N>h, design: <N>h, code: <N>h)
Top 5 Refactoring Candidates:
  1. [<effort>] <description> — <file>
  2. [<effort>] <description> — <file>
  3. [<effort>] <description> — <file>
  4. [<effort>] <description> — <file>
  5. [<effort>] <description> — <file>
Test Safety: <N>/<M> candidates have tests
Overall Grade: <A-F> (<N>/100)
Graph Nodes Created: <N> tech-debt tasks

Saved to memory: "Tech Debt Audit — <date>"
```

## Anti-Patterns

- Do NOT refactor without tests — add tests first, then refactor
- Do NOT refactor during bug fixes — separate commits, separate concerns
- Do NOT chase 100% — diminishing returns after 80%, focus on high-impact debt
- Do NOT refactor code that works and rarely changes — prioritize high-churn hotspots
- Do NOT ignore hotspots — frequently changed files deserve clean code the most
- Do NOT plan large refactors — break into atomic tasks tracked in the execution graph


## Codex Notes

- In Codex Plan Mode, use this skill for planning only and do not mutate files.
- During implementation, follow the project `AGENTS.md` rules and use `apply_patch` for manual edits.

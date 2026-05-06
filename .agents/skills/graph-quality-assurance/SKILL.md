---
name: graph-quality-assurance
description: Code quality audit using Clean Code (Uncle Bob), SOLID principles, DRY analysis, McCabe complexity, and project convention checks
triggers:
  - graph-quality-assurance
version: 1.0.0
author: Diego Nogueira
date: 2026-04-04
---

# graph-quality-assurance

Code quality audit using Clean Code (Uncle Bob), SOLID principles, DRY analysis, McCabe cyclomatic complexity, and project convention checks. Produces a scored report across 7 quality dimensions.

## When to Use

- After IMPLEMENT phase, before REVIEW phase
- During refactoring to measure improvement
- When code quality is a concern or technical debt is accumulating
- The user says "quality audit", "code review", "check quality", or "clean code"

## Mandatory Flow

```
lint → typecheck → code smells → SOLID → DRY → complexity → conventions → report → write_memory
```

## Workflow

### Step 1: Lint Gate

Run the project linter:
```bash
npm run lint
```

Verify:
- Zero errors
- Warnings within max-warnings threshold (9)
- No new violations introduced by recent changes
- Score: 100 if zero errors + within threshold, -10 per error, -2 per warning over threshold

### Step 2: Type Safety Gate

Run TypeScript strict type checking:
```bash
npm run typecheck
```

Verify:
- Zero TypeScript errors
- No `@ts-ignore` or `@ts-expect-error` without explicit approval
- No `any` types in production code (tests may use sparingly)
- Score: 100 if zero errors, -5 per error, -10 per `any` in production code

### Step 3: Code Smells Detection

Analyze modified/new files for common code smells:

| Smell | Threshold | Severity |
|-------|-----------|----------|
| Long functions | > 50 lines | High |
| Deep nesting | > 3 levels | High |
| God classes | > 300 lines | High |
| Feature envy | Method uses more data from another class than its own | Medium |
| Data clumps | Same group of params repeated in 3+ functions | Medium |
| Primitive obsession | Using primitives instead of domain types | Low |
| Dead code | Unused exports, unreachable branches | Medium |
| Commented-out code | Code blocks in comments | Low |

Score: 100 baseline, -5 per low, -10 per medium, -15 per high smell found.

### Step 4: SOLID Principles Check

Evaluate modified modules against SOLID:

| Principle | Check | How to Verify |
|-----------|-------|---------------|
| **S** — Single Responsibility | One reason to change per module | Count distinct responsibilities; flag if > 1 |
| **O** — Open/Closed | Extend via composition, not modification | Check for switch/if chains that grow with new types |
| **L** — Liskov Substitution | Subtypes substitutable for base types | Verify interface implementations don't throw unexpected errors |
| **I** — Interface Segregation | No fat interfaces | Flag interfaces with > 7 methods or unused method implementations |
| **D** — Dependency Inversion | Depend on abstractions, not concretions | Check for direct instantiation of dependencies vs injection |

Score: 20 points per principle adhered to (100 max). -10 per violation found.

### Step 5: DRY Analysis

Detect code duplication in modified files:

- Scan for identical or near-identical code blocks (> 5 lines) across files
- Identify copy-paste candidates for extraction into shared utilities
- Check for repeated patterns that could use generics or higher-order functions
- Flag string literals repeated > 3 times without constants

Score: 100 if no duplication, -10 per duplicated block, -5 per repeated literal.

### Step 6: Complexity Gate

McCabe cyclomatic complexity analysis on modified functions:

| Complexity | Rating | Action |
|------------|--------|--------|
| 1-5 | Low | No action needed |
| 6-10 | Moderate | Monitor, consider simplifying |
| 11-20 | High | **Flagged** — should be simplified |
| > 20 | Very High | **Required refactoring** — must decompose |

Count decision points: `if`, `else if`, `case`, `while`, `for`, `&&`, `||`, `catch`, `?:`.

Score: 100 if all functions <= 10, -5 per function 11-20, -15 per function > 20.

### Step 7: Convention Compliance

Verify project conventions from CLAUDE.md:

| Convention | Rule | Check |
|------------|------|-------|
| File naming | Kebab-case | `graph-store.ts`, not `graphStore.ts` |
| Type naming | PascalCase | `GraphNode`, `NodeStatus` |
| Function naming | camelCase | `findNextTask()`, `buildTaskContext()` |
| Imports | ESM with `.js` extension | `import { foo } from './bar.js'` |
| Zod imports | From `'zod/v4'` | Never from `'zod'` |
| Exports | Named only | No `export default` |
| Logging | Project logger | No `console.log` in production code |
| Errors | Typed errors | No raw `throw "string"` or `throw new Error("msg")` without custom class |

Score: 100 baseline, -5 per convention violation.

### Step 8: Quality Report

Calculate overall score and grade:

| Dimension | Weight | Score |
|-----------|--------|-------|
| Lint | 15% | 0-100 |
| Type Safety | 15% | 0-100 |
| Code Smells | 15% | 0-100 |
| SOLID | 15% | 0-100 |
| DRY | 10% | 0-100 |
| Complexity | 15% | 0-100 |
| Conventions | 15% | 0-100 |

**Grades:** A (85-100), B (70-84), C (55-69), D (40-54), F (< 40).

Save report:
```
Tool: mcp__mcp-graph__write_memory
Params:
  title: "Quality Audit — <date>"
  content: "<scores per dimension, overall grade, top issues, recommendations>"
  tags: ["quality", "audit", "clean-code", "solid"]
```

## Output Format

```
Phase: QUALITY ASSURANCE
Lint:        score/100 (N errors, N warnings)
Type Safety: score/100 (N errors, N any-types)
Code Smells: score/100 (N high, N medium, N low)
SOLID:       score/100 (N violations)
DRY:         score/100 (N duplications)
Complexity:  score/100 (N functions > 10, N functions > 20)
Conventions: score/100 (N violations)

Overall: score/100 — Grade X
Top Issues: <top 3 findings>
Recommendations: N action items

Saved to memory: "Quality Audit — <date>"
```

## Anti-Patterns

- Do NOT ignore lint warnings — they indicate real issues that compound over time
- Do NOT use `@ts-ignore` without explicit user approval — fix the type error instead
- Do NOT skip typecheck — type safety is a core project requirement
- Do NOT add `console.log` — use the project logger from `src/core/utils/logger.ts`
- Do NOT create functions > 50 lines without decomposing — extract helpers
- Do NOT use `any` type — use `unknown` with type guards or proper generics
- Do NOT skip convention checks for "quick fixes" — conventions prevent accumulated tech debt


## Codex Notes

- In Codex Plan Mode, use this skill for planning only and do not mutate files.
- During implementation, follow the project `AGENTS.md` rules and use `apply_patch` for manual edits.

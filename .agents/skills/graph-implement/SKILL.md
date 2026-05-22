---
name: graph-implement
description: Execute the IMPLEMENT phase of mcp-graph lifecycle — TDD Red-Green-Refactor with Granular flow, 8 DoD checks, epic promotion
triggers:
  - graph-implement
version: 2.0.0
author: Diego Nogueira
date: 2026-04-04
---

# graph-implement

Execute the IMPLEMENT phase of the mcp-graph lifecycle. Primary coding phase — every line of code follows TDD Red-Green-Refactor and is tracked in the graph via v6.0 pipeline tools.

## Core Rule

**No node in the graph = no code written.** Test before code. No test = no implementation.

## When to Use

- After PLAN phase is complete (tasks decomposed, sprints planned)
- Implementing the next available task from the graph
- The `_lifecycle.phase` returned by mcp-graph is `IMPLEMENT`
- User says "next task", "implement", or "start coding"

## Mandatory Flow

**v8.0 Granular (MANDATORY — 6 calls):**
```
next → context(compact) → context(rag) → update_status(in_progress) → [TDD] → analyze(implement_done) → update_status(done)
```

**⚠️ Pipeline v6.0 (FORBIDDEN):**
Do NOT use `start_task` or `finish_task` as they create unwanted `ai-shadow` branches.

## Workflow

### Step 1: Pick Next Task & Load Context

**v11 surface (preferred):**
```
/next                   # Claude skill — picks up next recommended task
mg next                 # shell — same handler
```

**Legacy surface (granular):**
```
Tool: mcp__mcp-graph__next
Tool: mcp__mcp-graph__context (id: <node_id>, action: "compact")
Tool: mcp__mcp-graph__context (action: "rag", query: <task details>)
Tool: mcp__mcp-graph__update_status (id: <node_id>, status: "in_progress")
```

Returns: task (id, title, AC, xpSize) + context + ragContext.

Display: title, id, priority, xpSize, acceptanceCriteria.

If task is blocked, display blockers and ask user to resolve or skip.

### Step 2: Pre-Implementation Checks

**TDD adherence check:**
```
Tool: mcp__mcp-graph__analyze (mode: "tdd_check", nodeId: <node_id>)
```

**Code sync check (detect stale refs):**
```
Tool: mcp__mcp-graph__analyze (mode: "code_sync")
```

### Step 3: TDD Red-Green-Refactor

Use the **tddHints** from `next` (or inferred from ACs) to guide test structure.

**RED — Write failing test first:**
1. Read existing codebase to understand APIs and patterns
2. Write the test file based on acceptance criteria + tddHints
3. Run the test — it MUST fail (confirms the test is meaningful)

**GREEN — Write minimal implementation:**
1. Write only enough code to make the test pass
2. Run the test — it MUST pass
3. Do not add features beyond what the test requires

**REFACTOR — Clean up:**
1. Improve code quality without changing behavior
2. Run the test again — it MUST still pass
3. Run the full test suite — no regressions

### Step 4: Finish Task (Granular)

Run full test suite first:
```bash
npx vitest run
```

Then finish via granular calls:

1. **Validate ACs:**
```
Tool: mcp__mcp-graph__validate (action: "ac", nodeId: <node_id>)
```

2. **Check DoD:**
```
Tool: mcp__mcp-graph__analyze (mode: "implement_done", nodeId: <node_id>)
```

3. **Mark Done:**
```
Tool: mcp__mcp-graph__update_status
Params:
  id: <node_id>
  status: "done"
  rationale: "<what was implemented, key decisions>"
```

### Step 5: Follow `_lifecycle.nextAction`

Every mcp-graph response includes `_lifecycle.nextAction`. Follow it:
- After `update_status(done)` → `next` (recommended) — restart from Step 1
- After `analyze(implement_done)` (fail) → fix blockers (required) — fix and retry Step 4
- Re-run `analyze(implement_done)` after fixing

## Definition of Done — 9 Checks

| # | Check | Severity | What it verifies |
|---|-------|----------|-----------------|
| 1 | `has_acceptance_criteria` | **required** | Task or parent has AC |
| 2 | `ac_quality_pass` | **required** | AC score ≥ 60 (INVEST) |
| 3 | `no_unresolved_blockers` | **required** | No depends_on to non-done nodes |
| 4 | `status_flow_valid` | **required** | Passed through in_progress before done |
| 5 | `has_description` | recommended | Non-empty description |
| 6 | `not_oversized` | recommended | Not L/XL without subtasks |
| 7 | `has_testable_ac` | recommended | ≥1 AC is testable |
| 8 | `has_estimate` | recommended | xpSize or estimateMinutes set |
| 9 | `has_test_files` | recommended | testFiles populated |

**Grades:** A (85-100%), B (70-84%), C (55-69%), D (<55%). Target: Grade A.

## Error Recovery

- **DoD gate fails:** Display failed checks. Common fixes:
  - `ac_quality_pass` → Update AC with concrete assertions via `node (action: "update")`
  - `has_testable_ac` → Rewrite ACs with measurable outcomes
  - `no_unresolved_blockers` → Resolve blockers or update their status
  - Re-run `analyze(implement_done)` after fixing
- **Blocked task:** Display blockers, ask to resolve or skip to next unblocked
- **Missing node:** Create via `node (action: "add")` BEFORE any code
- **Test failures:** Diagnose root cause before retrying. Never skip failing tests
- **Epic promotion:** If `update_status(done)` rationale indicates all children are done, inform user

## Output Format

```
Task: <title> (<node_id>)
Phase: IMPLEMENT
Grade: <A/B/C/D> (score: <N>/100)
Tests: <N> passed, <N> failed
Epic promotion: <yes/no>
Next: <next_task_title> (<next_id>)

Run $graph-implement to continue.
```

## XP Anti-Vibe-Coding Principles

1. **TDD mandatory** — Test before code. No test = no implementation.
2. **Anti-one-shot** — Never generate entire systems in one prompt. One task at a time.
3. **Atomic decomposition** — Each task completable in ≤ 2h.
4. **Code detachment** — If the AI made an error, explain via prompt. Never edit manually.

## Anti-Patterns

- Do NOT use the pipeline v6.0 flow — use Granular flow v8.0 instead
- Do NOT ignore `_lifecycle.nextAction` — it guides the optimal next action
- Do NOT use deprecated tool names (`add_node`, `update_node`) — use `node (action: "add"|"update")`
- Do NOT skip `analyze(implement_done)` DoD checks — they enforce quality gates
- Do NOT mark done without running the full test suite first
- Do NOT implement without loading context — use `context(compact)` and `context(rag)`


## Codex Notes

- In Codex Plan Mode, use this skill for planning only and do not mutate files.
- During implementation, follow the project `AGENTS.md` rules and use `apply_patch` for manual edits.

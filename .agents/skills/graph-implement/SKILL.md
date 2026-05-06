---
name: graph-implement
description: Execute the IMPLEMENT phase of mcp-graph lifecycle — TDD Red-Green-Refactor with v6.0 pipeline (start_task/finish_task), 9 DoD checks, epic promotion
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

**v6.0 Pipeline (PRIMARY — 2 calls):**
```
start_task → [TDD Red-Green-Refactor] → finish_task
```

**v5.x Granular (FALLBACK — 6 calls, for fine control):**
```
next → context → context(action: "rag") → update_status(in_progress) → [TDD] → analyze(implement_done) → update_status(done)
```

## Workflow

### Step 1: Start Task (v6.0 Pipeline)

**v11 surface (preferred — installs `@mcp-graph-workflow/cli@beta`):**
```
/start                  # Claude skill — picks up next task or pass <id>
mg start [<id>]         # shell — same handler, also works in CI/scripts
```

**Legacy surface (still works, slower round-trip via MCP):**
```
Tool: mcp__mcp-graph__start_task
Params: contextDetail: "standard", ragBudget: 4000, autoStart: true
```

Either form executes: `next` + `context` + `context(action: "rag")` + `update_status(in_progress)`.

Returns: task (id, title, AC, xpSize) + context + ragContext + **tddHints** + startedAt.

Display: title, id, priority, xpSize, acceptanceCriteria, tddHints, enhancedReason.

If task is blocked, display blockers and ask user to resolve or skip.

To target a specific task: `start_task(nodeId: "<id>")`.

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

Use the **tddHints** from `start_task` to guide test structure.

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

### Step 4: Finish Task (v6.0 Pipeline)

Run full test suite first:
```bash
npx vitest run
```

Then finish via pipeline:

**v11 surface (preferred):**
```
/finish                 # Claude skill — auto-detects in_progress task
mg finish [<id>]        # shell — same handler
```

**Legacy surface (still works, slower round-trip via MCP):**
```
Tool: mcp__mcp-graph__finish_task
Params:
  nodeId: <node_id>
  rationale: "<what was implemented, key decisions>"
  testFiles: ["src/tests/<test-file>.test.ts"]
  autoNext: true
```

Either form automatically executes:
- **DoD 9 checks** (see table below)
- AC validation
- `update_status(done)`
- Epic promotion check (suggests promoting parent when all children done)
- Returns next task (if `autoNext: true`)

### Step 5: Follow `_lifecycle.nextAction`

Every mcp-graph response includes `_lifecycle.nextAction`. Follow it:
- After `finish_task` (pass) → `start_task` (recommended) — restart from Step 1
- After `finish_task` (fail) → fix blockers (required) — fix and retry Step 4

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
  - Re-run `finish_task` after fixing
- **Blocked task:** Display blockers, ask to resolve or skip to next unblocked
- **Missing node:** Create via `node (action: "add")` BEFORE any code
- **Test failures:** Diagnose root cause before retrying. Never skip failing tests
- **Epic promotion:** If `finish_task` returns `epicPromotion.readyToPromote: true`, inform user

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

- Do NOT use the granular v5.x flow when pipeline v6.0 is available — use `start_task`/`finish_task`
- Do NOT ignore `_lifecycle.nextAction` — it guides the optimal next action
- Do NOT use deprecated tool names (`add_node`, `update_node`) — use `node (action: "add"|"update")`
- Do NOT skip `finish_task` DoD checks — they enforce quality gates
- Do NOT mark done without running the full test suite first
- Do NOT implement without loading context — `start_task` does this automatically


## Codex Notes

- In Codex Plan Mode, use this skill for planning only and do not mutate files.
- During implementation, follow the project `AGENTS.md` rules and use `apply_patch` for manual edits.

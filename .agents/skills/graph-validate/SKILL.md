---
name: graph-validate
description: Execute the VALIDATE phase of mcp-graph lifecycle — unified validate API, done integrity, scenario coverage, DORA quality metrics
triggers:
  - graph-validate
version: 2.0.0
author: Diego Nogueira
date: 2026-04-04
---

# graph-validate

Execute the VALIDATE phase of the mcp-graph lifecycle. Runs comprehensive validation: E2E tests, acceptance criteria verification, integrity checks, and quality metrics via DORA.

## When to Use

- After IMPLEMENT phase has completed a sprint's worth of tasks
- Verifying that all acceptance criteria are met across multiple tasks
- Running E2E tests that span multiple components
- The `_lifecycle.phase` returned by mcp-graph is `VALIDATE`

## Mandatory Flow

```
validate(task) → validate(ac) → analyze(done_integrity) → analyze(status_flow) → analyze(scenario_coverage) → forecast(dora) → metrics → analyze(validate_ready) → set_phase(REVIEW)
```

## Workflow

### Step 1: Identify Scope

List all tasks marked `done` in the current sprint:
```
Tool: mcp__mcp-graph__list (filter by sprint and status: "done")
```

### Step 2: Browser Validation (per task with UI)

For each completed task with UI components:
```
Tool: mcp__mcp-graph__validate (action: "task", url: "<app_url>", nodeId: <task_id>)
```

For A/B comparison (before/after):
```
Tool: mcp__mcp-graph__validate (action: "task", url: "<new_url>", compareUrl: "<old_url>", selector: "<css_selector>")
```

Captured content auto-indexes into Knowledge Store.

### Step 3: Validate Acceptance Criteria Quality

```
Tool: mcp__mcp-graph__validate (action: "ac")
```

Checks all nodes with AC: quality scoring (INVEST), measurability bonus.

### Step 4: Run Full Test Suite

```bash
npx vitest run
```

For E2E tests:
```bash
npx vitest run tests/integration/
```

Verify: all unit + integration tests pass, no regressions.

### Step 5: Verify Done Integrity

```
Tool: mcp__mcp-graph__analyze (mode: "done_integrity")
```

Checks that all `done` nodes actually meet Definition of Done (9 checks).

### Step 6: Validate Status Flow

```
Tool: mcp__mcp-graph__analyze (mode: "status_flow")
```

Verifies all nodes followed valid status transitions (e.g., went through `in_progress` before `done`).

### Step 7: Check Scenario Coverage

```
Tool: mcp__mcp-graph__analyze (mode: "scenario_coverage")
```

Validates that user scenarios have been covered by implemented tasks.

### Step 8: DORA Quality Metrics

```
Tool: mcp__mcp-graph__forecast (mode: "dora")
```

Review:
- **Change Failure Rate** — status reversals / total done (target: < 5%)
- **Lead Time** — P85 hours from created to done (target: < 24h)
- **MTTR** — mean time to recover from rework (target: < 1h)

### Step 9: Collect Sprint Metrics

```
Tool: mcp__mcp-graph__metrics
```

Review: task completion rate, AC pass rate, avg completion time vs estimates.

### Step 10: Validate Gate

```
Tool: mcp__mcp-graph__analyze (mode: "validate_ready")
```

**Gate criteria:**
- All sprint tasks validated
- AC pass rate meets threshold
- No critical test failures
- done_integrity and status_flow checks pass

If validation fails, return to IMPLEMENT to fix issues.

### Step 11: Transition

Once gate passes:
```
Tool: mcp__mcp-graph__set_phase (phase: "REVIEW")
```

Follow `_lifecycle.nextAction` for the recommended next tool call.

## Output Format

```
Phase: VALIDATE → REVIEW
Tasks validated: N/M passed
Tests: N passed, M failed
AC pass rate: X%
DORA: change failure rate Y%, lead time P85 Zh
Gate: validate_ready — score N/100, grade X
Status: Ready to proceed to REVIEW phase
```

## Anti-Patterns

- Do NOT skip E2E tests — unit tests alone are insufficient
- Do NOT mark validation as passed if tests fail — fix first
- Do NOT ignore flaky tests — investigate and fix root cause
- Do NOT validate tasks still in_progress — complete first
- Do NOT ignore `_lifecycle.nextAction` — it guides the optimal next action
- Do NOT use deprecated `validate_task` — use `validate (action: "task")`
- Do NOT skip done_integrity check — it catches DoD violations missed during IMPLEMENT


## Codex Notes

- In Codex Plan Mode, use this skill for planning only and do not mutate files.
- During implementation, follow the project `AGENTS.md` rules and use `apply_patch` for manual edits.

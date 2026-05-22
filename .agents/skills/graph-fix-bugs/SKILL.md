---
name: graph-fix-bugs
description: Structured bug fix workflow using Root Cause Analysis (5 Whys), Reproduce-Fix-Verify cycle, TDD for bugs, and regression prevention
triggers:
  - graph-fix-bugs
version: 1.0.0
author: Diego Nogueira
date: 2026-04-04
---
> 💡 **v11 CLI surface available** (`@mcp-graph-workflow/cli@beta`): lifecycle verbs (`start_task`, `finish_task`, `next`, `update_status`, `list`, `add_node`, `set_phase`) referenced below have faster slash/shell equivalents — see **[V11-SURFACE-MAP.md](./V11-SURFACE-MAP.md)**. Prefer `/start` over `mcp__mcp-graph__start_task` when possible. Other tools (analyze, validate, search, edge, node, metrics, etc.) stay MCP for now.


# graph-fix-bugs

Structured bug fix workflow using Root Cause Analysis (5 Whys), Reproduce-Fix-Verify cycle, TDD for bugs, and regression prevention. Every bug fix follows a disciplined process that prevents recurrence.

## When to Use

- When a bug is identified (from graph-bug-hunter, user report, or test failure)
- During IMPLEMENT phase for bug-fix tasks
- When a regression is detected after a deployment or merge

## Mandatory Flow

```
select bug → start_task → reproduce (RED) → 5 Whys → impact analysis → fix (GREEN) → regression suite → verify AC → prevent → finish_task → write_memory
```

## Workflow

### Step 1: Bug Selection

List bugs from the graph:
```
Tool: mcp__mcp-graph__list (type: "task", tags: ["bug"])
```

Or search for specific bugs:
```
Tool: mcp__mcp-graph__search (query: "bug")
```

Select the highest priority unresolved bug. Load context via pipeline:
```
Tool: mcp__mcp-graph__start_task (nodeId: <bug_node_id>)
```

### Step 2: Reproduce (TDD RED)

Write a test that reproduces the bug. The test MUST fail — if it passes, the bug description is wrong or already fixed.

Test file naming convention: `src/tests/bug-fix-<description>.test.ts`

```bash
npx vitest run src/tests/bug-fix-<description>.test.ts
```

Confirm the test is RED. If the test passes, re-examine the bug report and update the node accordingly.

### Step 3: Root Cause Analysis (5 Whys)

Interactive investigation. Ask "Why?" 5 times to drill from symptom to root cause. Document each level:

1. **Why** does X fail? → Because Y is null
2. **Why** is Y null? → Because Z doesn't initialize it
3. **Why** doesn't Z initialize it? → Because the constructor skips that branch
4. **Why** does it skip that branch? → Because the condition check is inverted
5. **Why** is the condition inverted? → Root cause: copy-paste error from similar function

Document the full chain in the bug node description.

### Step 4: Impact Analysis

Understand the blast radius before changing code:
```
Tool: mcp__mcp-graph__code_intelligence (action: "impact", symbol: "<affected_module>", depth: 3)
```

Determine:
- How many modules depend on the buggy code?
- Will the fix break callers?
- Are there other call sites with the same bug pattern?

### Step 5: Fix Implementation (TDD GREEN)

Write the minimal fix to make the failing test pass.

Rules:
- Do NOT refactor during fix — keep the change as small as possible
- Do NOT add features — fix only the bug
- Do NOT change unrelated code — minimize the diff

Run the bug test — it MUST pass now:
```bash
npx vitest run src/tests/bug-fix-<description>.test.ts
```

### Step 6: Regression Suite

Run the full test suite — ALL existing tests must still pass:

```bash
npm test
```

Zero regressions allowed. If any test breaks, the fix is too broad — narrow it down. A bug fix that introduces new failures is not a fix.

### Step 7: Verification

Verify fix against the bug node's acceptance criteria:
```
Tool: mcp__mcp-graph__validate (action: "ac", nodeId: <bug_node_id>)
```

Confirm the original user-reported behavior is fixed. If the bug has a UI component, use browser validation:
```
Tool: mcp__mcp-graph__validate (action: "task", url: "<app_url>", nodeId: <bug_node_id>)
```

### Step 8: Prevention

Document the bug pattern in knowledge store:
```
Tool: mcp__mcp-graph__write_memory
```

Include in the memory:
- **Root cause** — the 5 Whys chain
- **Symptoms** — how the bug manifested
- **Fix approach** — what was changed and why
- **Prevention strategy** — how to avoid this pattern in the future

This enables future graph-bug-hunter scans to detect similar patterns automatically.

### Step 9: Close Bug

Finish the task via pipeline:
```
Tool: mcp__mcp-graph__finish_task (nodeId: <bug_node_id>, rationale: "<root cause + fix summary>", testFiles: ["src/tests/bug-fix-<name>.test.ts"])
```

This automatically:
- Runs DoD 9 checks
- Marks status as `done`
- Checks epic promotion
- Returns next task

## Output Format

```
Bug Fix Report
==============
Bug: <title> (<node_id>)
Root cause: <1-line summary from 5 Whys>
Blast radius: N modules affected
Fix: <files changed>, <lines changed>
Tests: N new + M existing passing
Prevention: Pattern documented in knowledge store
DoD: Grade <A/B/C/D> (score: N/100)
```

## Anti-Patterns

- Do NOT fix without reproducing first — write the failing test before touching production code
- Do NOT fix multiple bugs in one commit — one bug, one fix, one commit
- Do NOT refactor during bug fix — separate commits for fix and refactor
- Do NOT skip 5 Whys — surface-level fixes recur within weeks
- Do NOT ignore blast radius — fixes can introduce new bugs in dependent modules
- Do NOT skip regression suite — run ALL tests, not just the bug test
- Do NOT close without documenting prevention — the team needs to learn from every bug
- Do NOT fix bugs not tracked in the graph — create the node first, then fix


## Codex Notes

- In Codex Plan Mode, use this skill for planning only and do not mutate files.
- During implementation, follow the project `AGENTS.md` rules and use `apply_patch` for manual edits.

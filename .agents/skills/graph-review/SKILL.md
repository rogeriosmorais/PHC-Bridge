---
name: graph-review
description: Execute the REVIEW phase of mcp-graph lifecycle — Code Intelligence blast radius, code-aware sync, mermaid visualization, quality feedback
triggers:
  - graph-review
version: 2.0.0
author: Diego Nogueira
date: 2026-04-04
---

# graph-review

Execute the REVIEW phase of the mcp-graph lifecycle. Performs code review with real blast radius analysis via Code Intelligence, detects stale references, and ensures quality standards before handoff.

## When to Use

- After VALIDATE phase has confirmed all tests pass
- Reviewing code changes before creating PR
- Analyzing blast radius of changes
- The `_lifecycle.phase` returned by mcp-graph is `REVIEW`

## Mandatory Flow

```
code_intelligence(impact) → analyze(code_sync) → export(mermaid) → metrics → knowledge_feedback → analyze(review_ready) → set_phase(HANDOFF)
```

## Workflow

### Step 1: Blast Radius Analysis (Code Intelligence)

```
Tool: mcp__mcp-graph__code_intelligence (action: "impact", symbol: "<changed_module>", depth: 3)
```

For each modified module, get real upstream/downstream dependents. This replaces manual blast radius guessing.

### Step 2: Code-Aware Sync Check

```
Tool: mcp__mcp-graph__analyze (mode: "code_sync")
```

Detects:
- **Stale sourceRefs** — nodes referencing deleted/moved files
- **Missing testFiles** — done tasks without test file references
- **Symbol drift** — code changed since graph was last updated

Fix any issues found before proceeding.

### Step 3: Visualize Graph State

```
Tool: mcp__mcp-graph__export (format: "mermaid")
```

Review the execution graph visually. For tabular data:
```
Tool: mcp__mcp-graph__export (format: "csv")
```

### Step 4: Collect Metrics

```
Tool: mcp__mcp-graph__metrics
```

Review: velocity, quality (AC pass rates), complexity (task sizes, dependency depth).

### Step 5: Code Review

Review all changes from the sprint:
```bash
git diff main...HEAD
git log main..HEAD --oneline
```

Check for:
- Code quality and readability
- Security vulnerabilities (OWASP top 10)
- Performance concerns
- Error handling completeness
- Test quality (not just coverage)
- Adherence to ADR decisions from DESIGN phase

### Step 6: Knowledge Quality Feedback

Provide feedback on RAG knowledge used during implementation:
```
Tool: mcp__mcp-graph__knowledge_feedback (action: "helpful|unhelpful|outdated", docId: "<id>")
```

This improves future RAG retrieval quality.

### Step 7: Validate Gate

```
Tool: mcp__mcp-graph__analyze (mode: "review_ready")
```

**Gate criteria:**
- All sprint tasks validated
- Code review completed (blast radius analyzed)
- No stale references (code_sync clean)
- Metrics within acceptable ranges

If validation fails, return to IMPLEMENT or VALIDATE to fix issues.

### Step 8: Transition

Once gate passes:
```
Tool: mcp__mcp-graph__set_phase (phase: "HANDOFF")
```

Follow `_lifecycle.nextAction` for the recommended next tool call.

## Output Format

```
Phase: REVIEW → HANDOFF
Blast radius: N modules affected (Code Intelligence)
Code sync: M stale refs, K missing testFiles
Changes: J files modified, L lines changed
Gate: review_ready — score N/100, grade X
Status: Ready to proceed to HANDOFF phase
```

## Anti-Patterns

- Do NOT skip Code Intelligence blast radius — manual analysis misses transitive dependencies
- Do NOT ignore code_sync warnings — stale refs cause confusion in future sprints
- Do NOT rubber-stamp reviews — actually read the diffs
- Do NOT skip ADR compliance check — design decisions exist for a reason
- Do NOT ignore `_lifecycle.nextAction` — it guides the optimal next action
- Do NOT forget knowledge_feedback — it improves RAG quality for the team


## Codex Notes

- In Codex Plan Mode, use this skill for planning only and do not mutate files.
- During implementation, follow the project `AGENTS.md` rules and use `apply_patch` for manual edits.

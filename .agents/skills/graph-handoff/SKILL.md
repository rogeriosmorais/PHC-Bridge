---
name: graph-handoff
description: Execute the HANDOFF phase of mcp-graph lifecycle — PR creation, write_memory, export_knowledge, doc completeness validation
triggers:
  - graph-handoff
version: 2.0.0
author: Diego Nogueira
date: 2026-04-04
---

# graph-handoff

Execute the HANDOFF phase of the mcp-graph lifecycle. Creates PRs, captures technical decisions as memories, shares knowledge, and finalizes documentation for delivery.

## When to Use

- After REVIEW phase has approved the changes
- Creating a PR for the sprint's work
- Capturing decisions and knowledge for future cycles
- The `_lifecycle.phase` returned by mcp-graph is `HANDOFF`

## Mandatory Flow

```
write_memory → snapshot → export → export_knowledge → [create PR] → analyze(doc_completeness) → analyze(handoff_ready) → set_phase(DEPLOY)
```

## Workflow

### Step 1: Capture Technical Decisions

Save key decisions and patterns discovered during implementation:
```
Tool: mcp__mcp-graph__write_memory (category: "decision")
```

Record: architectural choices, error patterns, performance insights, integration learnings.

### Step 2: Create Snapshot

```
Tool: mcp__mcp-graph__snapshot
```

Captures the current state of the graph for audit trail.

### Step 3: Export Deliverables

```
Tool: mcp__mcp-graph__export
```

Generate summary of all completed work for PR description.

For visual overview:
```
Tool: mcp__mcp-graph__export (format: "mermaid")
```

### Step 4: Share Knowledge with Team

```
Tool: mcp__mcp-graph__export_knowledge
Params: sources: ["memory", "docs"], minQuality: 0.5, includeMemories: true
```

Creates a knowledge package for cross-project sharing. Deduplicates by content hash — safe to run multiple times.

### Step 5: Prepare Commit

Stage and commit all changes:
```bash
git status
git diff --staged
git log --oneline -5
```

Create a well-structured commit following project conventions.

### Step 6: Create Pull Request

If requested by user:
```bash
gh pr create --title "<PR title>" --body "<PR body>"
```

PR body should include:
- Summary of changes (from export)
- Tasks completed (node IDs and titles)
- Test results
- Breaking changes (if any)
- Mermaid graph visualization

### Step 7: Validate Documentation

```
Tool: mcp__mcp-graph__analyze (mode: "doc_completeness")
```

Verify all required documentation is updated (CLAUDE.md, ADRs, API docs).

### Step 8: Validate Gate

```
Tool: mcp__mcp-graph__analyze (mode: "handoff_ready")
```

**Gate criteria:**
- All sprint tasks are done
- Snapshot created
- Knowledge exported
- Memories captured
- PR created (if applicable)
- Documentation updated

### Step 9: Transition

Once gate passes:
```
Tool: mcp__mcp-graph__set_phase (phase: "DEPLOY")
```

Follow `_lifecycle.nextAction` for the recommended next tool call.

## Output Format

```
Phase: HANDOFF → DEPLOY
PR: #N (url)
Snapshot: created
Memories: N decisions captured
Knowledge: M docs exported
Gate: handoff_ready — score N/100, grade X
Status: Ready to proceed to DEPLOY phase
```

## Anti-Patterns

- Do NOT skip write_memory — decisions lost now are rediscovered expensively later
- Do NOT skip snapshots — they provide audit trail
- Do NOT create PRs without test results in the body
- Do NOT forget to update CLAUDE.md with new conventions
- Do NOT ignore `_lifecycle.nextAction` — it guides the optimal next action
- Do NOT skip export_knowledge — it enables cross-project learning via `learn_from_project`


## Codex Notes

- In Codex Plan Mode, use this skill for planning only and do not mutate files.
- During implementation, follow the project `AGENTS.md` rules and use `apply_patch` for manual edits.

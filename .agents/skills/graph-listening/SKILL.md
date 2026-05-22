---
name: graph-listening
description: Execute the LISTENING phase of mcp-graph lifecycle — DORA retrospective, knowledge gap analysis, CFD, cross-project learning, next cycle seeding
triggers:
  - graph-listening
version: 2.0.0
author: Diego Nogueira
date: 2026-04-04
---

# graph-listening

Execute the LISTENING phase of the mcp-graph lifecycle. Collects feedback, runs data-driven retrospectives via DORA, analyzes knowledge gaps, and seeds the next development cycle.

## When to Use

- After DEPLOY phase has released the changes
- Collecting feedback on delivered features
- Running sprint retrospective with real metrics
- The `_lifecycle.phase` returned by mcp-graph is `LISTENING`

## Mandatory Flow

```
forecast(dora) → analyze(cfd) → knowledge_stats → metrics → [collect feedback] → node(add) → learn_from_project → write_memory → analyze(backlog_health) → set_phase(ANALYZE)
```

## Workflow

### Step 1: DORA Retrospective (data-driven)

```
Tool: mcp__mcp-graph__forecast (mode: "dora")
```

Analyze sprint performance:
- **Deployment Frequency** — did we meet velocity targets?
- **Lead Time** — where were the bottlenecks?
- **Change Failure Rate** — quality trending up or down?
- **MTTR** — are we recovering faster from issues?
- **Trend** — overall trajectory (improving/stable/declining)

### Step 2: Cumulative Flow Analysis

```
Tool: mcp__mcp-graph__analyze (mode: "cfd")
```

Review flow patterns:
- WIP accumulation zones (bottleneck detection)
- Lead time distribution
- Flow efficiency (active time / total lead time, target > 40%)

### Step 3: Knowledge Gap Analysis

```
Tool: mcp__mcp-graph__knowledge_stats
```

Review: docs by source, quality distribution, top accessed docs, staleness.
Identify areas where knowledge is thin or stale.

### Step 4: Sprint Metrics

```
Tool: mcp__mcp-graph__metrics
```

Analyze:
- Velocity trends (estimated vs actual)
- Quality metrics (bugs found, AC pass rates)
- Estimation accuracy
- Knowledge coverage gaps

### Step 5: Collect Feedback

Gather from: user testing, bug reports, performance observations, feature requests, tech debt.

### Step 6: Create Follow-Up Nodes

For each actionable feedback item:

**Bug fixes:**
```
Tool: mcp__mcp-graph__node (action: "add", type: "task", tags: ["bug"])
```

**Feature requests:**
```
Tool: mcp__mcp-graph__node (action: "add", type: "epic" or "task")
```

**Technical debt:**
```
Tool: mcp__mcp-graph__node (action: "add", type: "task", tags: ["tech-debt"])
```

### Step 7: Import Cross-Project Learnings

If other team projects have relevant knowledge:
```
Tool: mcp__mcp-graph__learn_from_project
Params: sourcePath: "<path>/workflow-graph/graph.db", categories: ["errors", "patterns", "estimates"]
```

Safe to call multiple times (deduplicates by content hash).

### Step 8: Record Retrospective Insights

```
Tool: mcp__mcp-graph__write_memory (category: "retro")
```

Record: what went well, what to improve, patterns discovered, DORA insights.

### Step 9: Check Backlog Health

```
Tool: mcp__mcp-graph__analyze (mode: "backlog_health")
```

Review: orphan nodes, stale tasks, blocked items, priority distribution.

### Step 10: New PRD (if applicable)

If feedback warrants a new feature area:
```
Tool: mcp__mcp-graph__import_prd
```

### Step 11: Transition

Start the next cycle:
```
Tool: mcp__mcp-graph__set_phase (phase: "ANALYZE")
```

Or if backlog already has planned work:
```
Tool: mcp__mcp-graph__set_phase (phase: "PLAN")
```

Follow `_lifecycle.nextAction` for the recommended next tool call.

## Output Format

```
Phase: LISTENING → ANALYZE (or PLAN)
DORA: freq X/day, lead P85 Yh, failure Z%, trend T
Feedback: N items processed
New nodes: M created (K bugs, J features, L tech-debt)
Knowledge: imported P docs from cross-project
Backlog health: score N/100
Status: Ready for next cycle
```

## Anti-Patterns

- Do NOT skip retrospectives — they drive continuous improvement
- Do NOT ignore technical debt — create nodes to track it
- Do NOT start coding immediately — feedback goes through ANALYZE/PLAN first
- Do NOT discard feedback — even deferred items should be tracked as nodes
- Do NOT ignore `_lifecycle.nextAction` — it guides the optimal next action
- Do NOT use deprecated tool names (`add_node`) — use `node (action: "add")`
- Do NOT skip DORA review — gut feelings are less reliable than data


## Codex Notes

- In Codex Plan Mode, use this skill for planning only and do not mutate files.
- During implementation, follow the project `AGENTS.md` rules and use `apply_patch` for manual edits.

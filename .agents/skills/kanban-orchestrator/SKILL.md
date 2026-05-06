---
name: kanban-orchestrator
description: Kanban board orchestration with WIP limits, auto-suggestions, bottleneck detection, and flow metrics. Deterministic-first approach inspired by Anthropic Agent Teams but 100% local-first.
triggers:
  - kanban-orchestrator
  - kanban
version: 1.0.0
author: Diego Nogueira
date: 2026-04-10
---

# kanban-orchestrator

Transform the mcp-graph execution graph into a visual Kanban board with intelligent orchestration. Deterministic-first: all decisions are made via graph traversal, no LLM calls needed.

## When to Use

- Visualizing project progress as a Kanban board
- Managing WIP (Work In Progress) limits
- Identifying bottlenecks and blocked tasks
- Getting smart suggestions for next actions
- Tracking flow metrics (throughput, cycle time, lead time)
- During any lifecycle phase (cross-cutting skill)

## MCP Tool Usage

### View the Kanban Board
```
kanban(action: "board")
kanban(action: "board", swimlane: "epic")
kanban(action: "board", swimlane: "sprint")
```

### Move a Card
```
kanban(action: "move", nodeId: "<id>", newStatus: "ready")
kanban(action: "move", nodeId: "<id>", newStatus: "in_progress")
kanban(action: "move", nodeId: "<id>", newStatus: "done")
```

### Get Suggestions
```
kanban(action: "suggestions")
```

## Dashboard

The **Kanban** tab in the dashboard provides:
- 5 status columns: Backlog, Ready, In Progress, Blocked, Done
- Drag-and-drop cards between columns
- WIP limit indicators (red when exceeded)
- Swimlane grouping by Epic or Sprint
- Suggestions sidebar with auto-apply
- Flow metrics bar (throughput, cycle time, blocked %)

## Methodology

### WIP Limits (Little's Law)
- Default: In Progress = 3, Ready = 10
- `cycle_time = WIP / throughput` — lower WIP = faster delivery
- Visual warning when limits are exceeded

### Pull System
- Use `next` tool or Kanban suggestions to pull the next task
- Never push tasks to In Progress without finishing current work

### Bottleneck-First (Theory of Constraints)
- If Blocked column is growing, stop adding new work
- Focus on resolving blockers before starting new tasks
- Automated detection when blocked > 30% of total tasks

### Suggestion Types
| Action | Trigger | Priority |
|--------|---------|----------|
| `unblock` | Blocked task with all deps resolved | 1 (urgent) |
| `wip_violation` | Column exceeds WIP limit | 1 (urgent) |
| `bottleneck_alert` | >30% tasks blocked | 1 (urgent) |
| `promote_ready` | Backlog task with all deps done | 2 (normal) |
| `start_next` | Recommended next task | 3 (low) |

### Flow Metrics
- **Throughput**: Total done tasks
- **Avg Cycle Time**: Average hours from creation to completion
- **Avg Lead Time**: Average hours from first status change to done
- **Blocked %**: Percentage of tasks currently blocked
- **WIP Violations**: Count of columns exceeding limits

## Integration with Other Skills

- **graph-implement**: Use `kanban(action: "board")` before `start_task` to see the big picture
- **graph-validate**: Check Kanban metrics after validation phase
- **graph-review**: Export Kanban state for review handoff
- **graph-plan**: Use swimlane view by sprint during planning

## Comparison with Anthropic Agent Teams

| Aspect | Anthropic | mcp-graph Kanban |
|--------|-----------|-----------------|
| Orchestration | LLM-heavy (expensive) | Deterministic-first (free) |
| Persistence | Context window | SQLite graph (permanent) |
| Visualization | None native | Dashboard + ASCII |
| WIP control | None | Built-in limits + alerts |
| Bottleneck detection | Manual | Automatic |


## Codex Notes

- In Codex Plan Mode, use this skill for planning only and do not mutate files.
- During implementation, follow the project `AGENTS.md` rules and use `apply_patch` for manual edits.

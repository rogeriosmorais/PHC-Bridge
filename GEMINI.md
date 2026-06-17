# Dual-Graph Knowledge & Execution Workflow

This repository uses a hybrid workflow that combines **Knowledge Graph Navigation** (via `graphify`) and **Execution Graph Management** (via `mcp-graph`).

## 1. Knowledge Navigation (`graphify`)
`graphify` is the primary tool for codebase navigation, architectural mapping, and understanding system-wide dependencies.

**Rules:**
- **Navigation First:** For any codebase question, always run `graphify query "<question>"` first. Use `graphify path` for relationships and `graphify explain` for symbols.
- **Deep Discovery:** Use `graphify-out/wiki/index.md` (if available) for broad navigation instead of raw source browsing.
- **Sync Requirement:** After any code modification, you MUST run `graphify update .` to keep the knowledge graph current (AST-only, low cost).

## 2. Execution Lifecycle (`mcp-graph`)
`mcp-graph` is the source of truth for task tracking and the implementation lifecycle.

**Rules:**
- **No Node = No Code:** Implementation is strictly forbidden unless a corresponding node exists in the execution graph.
- **Pipeline v8.0:** Use `start_task → [Implement] → finish_task` as the standard loop.
- **Definition of Done:** Mandatory 8-check DoD validation via `analyze(mode: "implement_done")` before finishing any task.

## 3. The Merged Cycle

| Lifecycle Phase | Knowledge Action (`graphify`) | Execution Action (`mcp-graph`) |
| :--- | :--- | :--- |
| **ANALYZE** | `query` to map existing features/gaps | `import_prd` / `add_node` (requirements) |
| **DESIGN** | `path` to analyze blast radius | `add_node` / `edge` (ADRs, interfaces) |
| **PLAN** | `explain` to verify component readiness | `plan_sprint` / `sync_stack_docs` |
| **IMPLEMENT** | `query` to load component mental model | `start_task` -> `finish_task` |
| **VALIDATE** | `update` to sync new code structures | `validate(ac)` / `metrics` |
| **REVIEW** | `query` to explain changes to reviewers | `export` / `analyze(review_ready)` |

> **Mandatory Command:** Always run `graphify update .` as the final step of `finish_task` or after any file-mutating operation.

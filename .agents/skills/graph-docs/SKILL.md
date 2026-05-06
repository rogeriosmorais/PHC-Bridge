---
name: graph-docs
description: Documentation health audit using JSDoc completeness, README freshness, example code validation, CLAUDE.md convention coverage, API documentation, and architecture doc generation from graph
triggers:
  - graph-docs
version: 1.0.0
author: Diego Nogueira
date: 2026-04-04
---

# graph-docs

Documentation health audit using JSDoc completeness, README freshness, example code validation, CLAUDE.md convention coverage, API documentation, and architecture doc generation from graph. Identifies documentation gaps, stale content, and drift between docs and code.

## When to Use

- Before HANDOFF phase, to ensure documentation is complete
- During quarterly doc reviews
- When onboarding new developers
- When CLAUDE.md needs updating after significant changes
- During LISTENING phase for documentation improvements

## Mandatory Flow

```
CLAUDE.md audit --> README check --> JSDoc coverage --> API docs --> example validation --> architecture docs --> changelog --> report --> write_memory
```

## Workflow

### Step 1: CLAUDE.md Audit

Verify CLAUDE.md covers all critical conventions:
- ESM imports (`.js` extension)
- Zod v4 (`import { z } from 'zod/v4'`)
- Strict mode (`strict: true`)
- Naming conventions (kebab-case files, PascalCase types, camelCase functions)
- Logger usage (never `console.log`)
- Testing rules (TDD, arrange-act-assert)
- Path-specific rules

Compare CLAUDE.md sections with actual codebase patterns. Flag: outdated conventions, missing new patterns, stale migration notes. Use `mcp__mcp-graph__analyze(mode:"doc_completeness")`.

### Step 2: README Freshness

Check README.md:
- Setup instructions work (`npm install` + `npm run dev`)
- Test command works (`npm test`)
- Build command works (`npm run build`)
- Architecture overview matches current structure
- Badge/status links are valid

Run setup commands to verify. Flag: broken commands, outdated screenshots, missing sections.

### Step 3: JSDoc Coverage

Scan all exported functions in `src/core/` and `src/mcp/` for JSDoc comments. Calculate coverage:

```
JSDoc coverage = functions with JSDoc / total exported functions
```

Check JSDoc quality:
- Has `@param` for each parameter
- Has `@returns`
- Has description

Flag: public functions without any documentation, `@param` types mismatching actual types.

### Step 4: API Documentation

For REST API routes (`src/api/routes/`):
- Verify each endpoint has method, path, description, request/response schema documented

For MCP tools (`src/mcp/tools/`):
- Verify each tool has description in `server.tool()` registration
- Verify parameter descriptions in Zod schemas

Cross-reference with `docs/reference/REST-API-REFERENCE.md` and `docs/reference/MCP-TOOLS-REFERENCE.md`.

### Step 5: Example Code Validation

Find all code examples in `docs/` and README. For each example:
- Verify syntax is valid (no broken imports)
- Verify referenced functions/modules still exist
- Verify the example output matches current behavior

Flag: examples importing deleted modules, examples with deprecated API calls.

### Step 6: Architecture Documentation

Generate/verify architecture docs from graph:
- Module dependency diagram via `mcp__mcp-graph__export(action:"mermaid")`
- Component inventory from `src/` directory structure
- Data flow from store --> core --> mcp --> cli layers

Compare with `docs/architecture/` files. Flag drift between documented and actual architecture.

### Step 7: Changelog Completeness

Verify CHANGELOG.md covers recent releases. Cross-reference with git tags and release-please entries. Check:
- Every `feat:`/`fix:` commit has corresponding changelog entry
- Breaking changes are highlighted
- Migration notes included for schema changes

### Step 8: Documentation Report

Compile the full audit report:

```
CLAUDE.md coverage: <N>%
README freshness: <days since last update>
JSDoc coverage: <N>%
API doc coverage: <N>%
Example validity: <N>%
Architecture drift items: <N>
Changelog completeness: <N>%
Top 5 gaps: <list>
Overall grade: <A-F>
```

**Grading:**
- **A (90-100):** All docs current, JSDoc > 80%, no stale examples, no drift
- **B (75-89):** Minor gaps, JSDoc > 60%, few stale examples
- **C (60-74):** CLAUDE.md outdated, JSDoc < 60%, some broken examples
- **D (45-59):** Significant gaps, many undocumented APIs, architecture drift
- **F (< 45):** Critical doc debt, broken README, no JSDoc, stale architecture

Save findings:
```
Tool: mcp__mcp-graph__write_memory
Params:
  title: "Documentation Audit — <date>"
  content: "<findings summary with coverage scores, gaps, recommendations>"
  tags: ["documentation", "audit", "jsdoc", "architecture"]
```

## Output Format

```
Phase: DOCUMENTATION AUDIT
CLAUDE.md coverage: <N>%
README freshness: <N> days
JSDoc coverage: <N>%
API doc coverage: <N>%
Example validity: <N>%
Architecture drift: <N> items
Changelog completeness: <N>%
Top 5 gaps: <list>
Overall grade: <A-F>

Saved to memory: "Documentation Audit — <date>"
```

## Anti-Patterns

- Do NOT treat docs as afterthought — write alongside code
- Do NOT let CLAUDE.md go stale — it's the AI pair programmer's primary context
- Do NOT skip JSDoc on public functions — they're the API contract
- Do NOT leave broken examples — they mislead developers
- Do NOT document what doesn't exist yet — document what IS
- Do NOT write docs without testing commands — broken setup instructions are worse than none
- Do NOT forget architecture docs — new developers need the big picture


## Codex Notes

- In Codex Plan Mode, use this skill for planning only and do not mutate files.
- During implementation, follow the project `AGENTS.md` rules and use `apply_patch` for manual edits.

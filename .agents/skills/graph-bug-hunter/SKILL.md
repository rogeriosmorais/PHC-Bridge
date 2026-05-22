---
name: graph-bug-hunter
description: Automated bug discovery through static analysis, LSP diagnostics, pattern detection, regression hotspot analysis, and error catalog mining
triggers:
  - graph-bug-hunter
version: 1.0.0
author: Diego Nogueira
date: 2026-04-04
---

# graph-bug-hunter

Automated bug discovery through static analysis, LSP diagnostics, pattern detection, regression hotspot analysis, and error catalog mining. Proactively finds bugs before they reach production.

## When to Use

- Before VALIDATE phase — catch bugs early before formal validation
- During code review — systematic bug detection across the codebase
- Proactively during LISTENING phase — periodic health checks
- When quality metrics decline — investigate root causes of quality degradation

## Mandatory Flow

```
LSP diagnostics → ESLint deep scan → pattern detection → dependency issues → regression hotspots → error catalog → bug triage → report → node(add) → write_memory
```

## Workflow

### Step 1: LSP Diagnostics Collection

Use TypeScript language server to collect all errors, warnings, info, and hints across the codebase.

```
Tool: mcp__mcp-graph__code_intelligence
```

Use symbol-level analysis to identify type errors, unresolved references, and signature mismatches.

Flag files with >3 diagnostics as high-attention targets.

Run `npx tsc --noEmit` to get the full diagnostic list from the compiler.

### Step 2: ESLint Deep Scan

Run ESLint with zero-warning tolerance:

```bash
npx eslint src/ --max-warnings 0
```

Focus areas:
- Security plugin warnings
- `no-non-null-assertion` violations
- `no-explicit-any` violations

Categorize findings by rule severity (error vs warning vs info).

### Step 3: Anti-Pattern Detection

Search for dangerous patterns in source code:

- Non-null assertions (`x!.prop`)
- `any` type usage
- Empty catch blocks (`catch {}` or `catch { }`)
- TODO/FIXME/HACK comments
- Magic numbers (hardcoded values without named constants)
- Unreachable code (code after `return`, `throw`, `break`)
- `console.log` in production code
- Synchronous file operations in async paths (`readFileSync` in async functions)

### Step 4: Dependency & Import Issues

Check for structural problems:

- Circular imports (A imports B imports A)
- Missing `.js` extensions in ESM imports
- Unused exports (dead code)
- Re-exports that break tree-shaking

Use graph dependency cycle detection:
```
Tool: mcp__mcp-graph__analyze (mode: "cycles")
```

### Step 5: Regression Hotspot Analysis

Find files with the most churn in the last 30 days:

```bash
git log --stat --since="30 days" -- src/
```

Files changed >5 times in 30 days are regression hotspots.

Cross-reference with test coverage — hotspots without tests are high-risk. These are the most likely source of future bugs.

### Step 6: Error Catalog Mining

Query knowledge store for known error patterns:
```
Tool: mcp__mcp-graph__context(action: "rag") (query: "error pattern bug fix")
```

Compare against current codebase for recurring issues. Check if previously fixed bugs have regressed.

### Step 7: Bug Triage

Classify all findings by severity:

| Severity | Criteria | Action |
|----------|----------|--------|
| **Critical** | Security vulnerability, data loss risk | Fix immediately |
| **High** | Wrong behavior, logic errors | Fix in current sprint |
| **Medium** | Code smell, maintainability risk | Schedule for next sprint |
| **Low** | Style, convention violations | Track and batch fix |

Create graph nodes for Critical and High severity bugs:
```
Tool: mcp__mcp-graph__node (action: "add", type: "task", tags: ["bug", "<severity>"])
```

Include reproduction steps and affected files in the node description.

### Step 8: Bug Report

Generate a summary report covering:

- Total findings by category and severity
- Hotspot files (most churn + most diagnostics)
- Recommended fix order (Critical first, then High)
- Estimated effort per severity level

Save the bug catalog to knowledge store:
```
Tool: mcp__mcp-graph__write_memory
```

## Output Format

```
Bug Hunt Report
===============
Total findings: N
  Critical: N | High: N | Medium: N | Low: N
Hotspot files: N (files with >5 changes in 30d)
New bug nodes created: N
Top 5 priority fixes:
  1. [Critical] <description> — <file>
  2. [Critical] <description> — <file>
  3. [High] <description> — <file>
  4. [High] <description> — <file>
  5. [High] <description> — <file>
```

## Anti-Patterns

- Do NOT ignore warnings — they become bugs over time
- Do NOT skip LSP diagnostics — type errors are bugs, not suggestions
- Do NOT treat TODO/FIXME as acceptable — track them as nodes in the graph
- Do NOT ignore regression hotspots — high-churn files predict future bugs
- Do NOT create bug nodes without severity and reproduction steps
- Do NOT skip error catalog check — past bugs recur in similar patterns
- Do NOT hunt bugs without running tests first — fix known failures before finding new ones


## Codex Notes

- In Codex Plan Mode, use this skill for planning only and do not mutate files.
- During implementation, follow the project `AGENTS.md` rules and use `apply_patch` for manual edits.

---
name: graph-performance
description: Performance engineering audit using Lighthouse, Web Vitals, N+1 query detection, memory profiling, and bundle size analysis
triggers:
  - graph-performance
version: 1.0.0
author: Diego Nogueira
date: 2026-04-04
---

# graph-performance

Performance engineering audit using Lighthouse, Web Vitals, N+1 query detection, memory profiling, and bundle size analysis. Identifies performance bottlenecks across build output, runtime behavior, database queries, memory usage, and frontend metrics.

## When to Use

- Before DEPLOY phase — ensure performance baselines are met
- After major feature implementation — detect regressions early
- When performance complaints arise — systematic root cause analysis
- During VALIDATE for UI features — Web Vitals and Lighthouse audit

## Mandatory Flow

```
build analysis --> bundle size --> runtime profiling --> N+1 detection --> memory check --> Web Vitals --> benchmark comparison --> report --> write_memory
```

## Workflow

### Step 1: Build Analysis

Run the full build and measure build time:

```bash
time npm run build
```

Analyze output:
- Record build duration (wall clock time)
- Check for TypeScript warnings or deprecation notices
- Compare with previous build times (check memory for baseline)
- Flag builds that take >60s as slow — investigate large modules or circular dependencies

### Step 2: Bundle Size Audit

Analyze the dist/ output size and composition:

```bash
du -sh dist/
```

Deep inspection:
- Check total bundle size — flag if >500KB
- Identify the largest files in dist/ by size
- Check for large dependencies that should be tree-shaken
- Verify no dev-only dependencies leak into production bundle
- Inspect import chains for unnecessary transitive dependencies
- Flag duplicate packages (same lib bundled multiple times)

### Step 3: Runtime Profiling

For Node.js backend:
- Check event loop lag and async operation timing
- Measure tool response times via `mcp__mcp-graph__metrics`
- Check RAG trace latency for query performance
- Flag any operation consistently >500ms

For dashboard (React SPA):
- Run Lighthouse audit (performance score, FCP, LCP, CLS)
- Check React render counts for unnecessary re-renders
- Verify lazy loading is effective for heavy tabs (Code Graph, React Flow)

### Step 4: N+1 Query Detection

Search for patterns that indicate N+1 queries in the codebase:

- Loops containing `db.prepare().get()` or `db.prepare().all()` inside iterations
- `store.getNodeById()` or `store.getEdgeById()` called inside `for`/`forEach`/`map` loops
- Sequential awaits on store methods inside iteration (`for...of` with `await store.X()`)
- Flag functions that make >5 DB calls in a single invocation

Check SQLite query patterns:
- Verify batch operations use `WHERE id IN (...)` instead of individual queries
- Check that transactions wrap multi-write operations
- Review migration queries for missing indexes on frequently queried columns

### Step 5: Memory & Resource Check

Check for memory leaks and unbounded resource usage:

- **Unbounded caches:** Verify all caches have `maxSize` or TTL (SemanticCache, ResponseCache, QueryCache patterns)
- **Growing Maps/Sets:** Check for Maps or Sets that grow without eviction — especially in long-running processes
- **Event listeners:** Verify all `on()`/`addEventListener()` have corresponding cleanup in `off()`/`removeEventListener()`
- **File handle leaks:** Check that all `fs.open()` calls have corresponding `close()`, or use `fs.readFile()`/`fs.writeFile()` instead
- **SQLite connections:** Verify `db.close()` is called on shutdown and in test cleanup
- **Stream handling:** Check that readable/writable streams are properly destroyed on error

### Step 6: Web Vitals (UI)

For the dashboard, measure Core Web Vitals against thresholds:

| Metric | Good | Needs Improvement | Poor |
|--------|------|-------------------|------|
| First Contentful Paint (FCP) | <1.8s | 1.8s-3.0s | >3.0s |
| Largest Contentful Paint (LCP) | <2.5s | 2.5s-4.0s | >4.0s |
| Cumulative Layout Shift (CLS) | <0.1 | 0.1-0.25 | >0.25 |
| Time to Interactive (TTI) | <3.9s | 3.9s-7.3s | >7.3s |

If Playwright is available, automate measurement:
```
Tool: mcp__playwright__browser_navigate (url: dashboard URL)
Tool: mcp__playwright__browser_evaluate (script: "performance.getEntriesByType('navigation')")
```

Check for layout shifts caused by async data loading without skeleton/placeholder states.

### Step 7: Benchmark Comparison

Compare current metrics against baselines:

- Run `npm run test:bench` if benchmark tests exist
- Check DORA metrics via `mcp__mcp-graph__forecast(mode:"dora")` for lead time and deployment frequency trends
- Compare build time, bundle size, and test duration with previous audit (from memory)
- Flag any metric with >20% regression from baseline

Establish new baseline if none exists. Save metrics for future comparison.

### Step 8: Performance Report

Score each dimension 0-100 and compute overall grade:

| Dimension | Weight | Score Criteria |
|-----------|--------|---------------|
| Build | 10% | Time, warnings, incremental support |
| Bundle | 20% | Size, tree-shaking, no duplicates |
| Runtime | 25% | Response times, event loop lag |
| Queries | 20% | N+1 count, missing indexes, batch usage |
| Memory | 15% | Leaks, cache bounds, cleanup |
| Web Vitals | 10% | FCP, LCP, CLS, TTI |

**Grading:**
- **A (90-100):** All metrics within thresholds, no N+1, no leaks
- **B (75-89):** Minor issues, 1-2 metrics slightly off
- **C (60-74):** Some N+1 queries or memory concerns, bundle slightly large
- **D (45-59):** Multiple dimensions failing, significant N+1 or leak issues
- **F (< 45):** Critical performance debt, blocking deployment

Save findings:
```
Tool: mcp__mcp-graph__write_memory (title: "Performance Audit — <date>", content: <report>)
```

## Output Format

```
Phase: PERFORMANCE AUDIT
Build: <N>s build time, <N> warnings
Bundle: <N>KB total (<N> files, largest: <name> at <N>KB)
Runtime: <N>/100 (avg response: <N>ms, p95: <N>ms)
N+1 Queries: <N> detected (<N> critical, <N> warning)
Memory: <N> issues (<N> unbounded caches, <N> leaked listeners)
Web Vitals: FCP <N>s, LCP <N>s, CLS <N>, TTI <N>s
Benchmark: <N> regressions (>20% from baseline)
Overall Grade: <A-F> (<N>/100)
Recommendations: <top 3 actions>

Saved to memory: "Performance Audit — <date>"
```

## Anti-Patterns

- Do NOT optimize without measuring first — profile before changing code
- Do NOT micro-optimize — focus on bottlenecks (Pareto 80/20), not hot loops that run once
- Do NOT ignore N+1 queries — they compound under load and are the #1 performance killer
- Do NOT skip memory check — leaks are silent until OOM crashes in production
- Do NOT deploy without bundle size check — bundle regression is common and cumulative
- Do NOT compare without baseline — use benchmark tests and previous audit data from memory


## Codex Notes

- In Codex Plan Mode, use this skill for planning only and do not mutate files.
- During implementation, follow the project `AGENTS.md` rules and use `apply_patch` for manual edits.

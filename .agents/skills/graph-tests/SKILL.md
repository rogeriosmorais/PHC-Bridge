---
name: graph-tests
description: Test strategy audit using Test Pyramid, FIRST principles, coverage analysis, and test quality assessment
triggers:
  - graph-tests
version: 1.0.0
author: Diego Nogueira
date: 2026-04-04
---

# graph-tests

Test strategy audit using Test Pyramid, FIRST principles, coverage analysis, and test quality assessment. Identifies gaps in test coverage, validates pyramid shape, and ensures TDD discipline across the codebase.

## When to Use

- After IMPLEMENT phase, to audit test quality before VALIDATE
- During VALIDATE phase, as part of comprehensive quality checks
- When test coverage is insufficient or declining
- Before major releases to ensure test confidence
- When onboarding new modules that lack test coverage

## Mandatory Flow

```
npm test --> coverage report --> pyramid check --> FIRST audit --> missing tests --> test quality --> edge cases --> report --> write_memory
```

## Workflow

### Step 1: Test Suite Gate

Run the full test suite. All tests must pass with zero failures.

```bash
npm test
```

If any test fails, STOP. Fix failures before proceeding with the audit. Never audit quality on a broken suite.

### Step 2: Coverage Report

Run coverage analysis and check against project thresholds:

```bash
npm run test:coverage
```

**Thresholds:**
- Statements: 70%
- Branches: 65%
- Functions: 70%
- Lines: 70%

Report all files below threshold. Identify the top 5 modules with lowest coverage as priority targets.

### Step 3: Test Pyramid Check

Count tests by type to verify pyramid shape:

- **Unit tests:** `src/tests/*.test.ts` without database/store dependencies
- **Integration tests:** Tests using `SqliteStore`, in-memory database, or cross-module interactions
- **E2E tests:** `src/tests/e2e/*.test.ts` (Playwright browser tests)

Verify pyramid shape: unit > integration > E2E. Flag inverted pyramids where integration or E2E tests outnumber unit tests.

**Healthy ratio target:** ~70% unit, ~20% integration, ~10% E2E.

### Step 4: FIRST Principles Audit

Audit a sample of test files against FIRST principles:

- **Fast:** Each test executes in < 1s? No unnecessary I/O or sleeps?
- **Independent:** No shared mutable state between tests? Each test creates its own store/state?
- **Repeatable:** Deterministic results, no external dependencies? No reliance on network or filesystem state?
- **Self-validating:** Clear assertions with descriptive messages? No manual inspection required?
- **Timely:** Written before/with implementation (TDD), not retroactively?

Score each principle 0-100. Overall FIRST score = average.

### Step 5: Missing Test Detection

For each modified `.ts` file in `src/core/` and `src/mcp/`, check if a corresponding `.test.ts` exists in `src/tests/`.

Flag modules without test coverage. For graph-tracked tasks, use:

```
Tool: mcp__mcp-graph__analyze (mode: "tdd_check")
```

List all public exported functions without corresponding test assertions.

### Step 6: Test Quality Check

Review test files for quality patterns:

- **Arrange-Act-Assert:** Each test follows the AAA structure?
- **Minimal mocks:** Prefer real instances (in-memory SQLite, temp files) over mocks?
- **Factory helpers:** Using `makeNode`, `makeEdge` from `src/tests/helpers/factories.ts`?
- **Descriptive names:** Test names describe behavior, not implementation? (e.g., `it('should return next unblocked task sorted by priority')`)
- **No test pollution:** Proper `beforeEach`/`afterEach` cleanup? No leaked state?
- **Single assertion focus:** Each test verifies one behavior?

### Step 7: Edge Case Coverage

For each function under test, verify coverage of:

- **Happy path:** Normal input produces expected output
- **Error paths:** Invalid input, null, undefined, empty strings
- **Boundary conditions:** 0, -1, MAX_SAFE_INTEGER, empty arrays, single-element arrays
- **Async error handling:** Rejected promises, timeout scenarios, concurrent access
- **Type edge cases:** Optional fields missing, extra fields present

Flag functions with only happy-path coverage.

### Step 8: Test Report

Compile the full audit report:

```
Test Suite: <N> tests, <N> passed, <N> failed
Coverage: statements <N>%, branches <N>%, functions <N>%, lines <N>%
Pyramid: unit <N> / integration <N> / e2e <N> (ratio: <X>:<Y>:<Z>)
FIRST Score: <N>/100 (F:<N> I:<N> R:<N> S:<N> T:<N>)
Gaps: <N> modules without tests
Quality: <N> issues found
Grade: <A-F>
```

**Grading:**
- **A (90-100):** All thresholds met, pyramid correct, FIRST > 80, no gaps
- **B (75-89):** Minor gaps, pyramid slightly off, FIRST > 65
- **C (60-74):** Coverage below threshold in some areas, pyramid inverted for some types
- **D (45-59):** Significant gaps, FIRST < 50, many modules untested
- **F (< 45):** Critical test debt, broken pyramid, widespread quality issues

Save findings:
```
Tool: mcp__mcp-graph__write_memory (title: "Test Audit Report — <date>", content: <report>)
```

## Output Format

```
Phase: TEST AUDIT
Tests: <N> passed, <N> failed
Coverage: <N>% statements, <N>% branches, <N>% functions, <N>% lines
Pyramid: unit:<N> integration:<N> e2e:<N>
FIRST Score: <N>/100
Gaps: <N> modules without tests
Grade: <A-F>
Recommendations: <top 3 actions>
```

## Anti-Patterns

- Do NOT skip running the full test suite — a passing suite is the baseline for any audit
- Do NOT mock what you can use in-memory — prefer `:memory:` SQLite over mocks for store tests
- Do NOT write tests after implementation — TDD first, always
- Do NOT use shared mutable state between tests — each test owns its state
- Do NOT ignore flaky tests — fix the root cause, never retry-and-hope
- Do NOT test implementation details — test behavior and public API contracts
- Do NOT skip edge cases for "happy path only" coverage — edge cases catch real bugs


## Codex Notes

- In Codex Plan Mode, use this skill for planning only and do not mutate files.
- During implementation, follow the project `AGENTS.md` rules and use `apply_patch` for manual edits.

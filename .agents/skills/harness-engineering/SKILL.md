---
name: harness-engineering
description: Evaluate project harnessability — composite agent-readiness metric across type coverage, test coverage, architecture fitness, and docs coverage. Run during VALIDATE and REVIEW phases.
triggers:
  - harness-engineering
  - harnessability
  - harness:scan
version: 1.0.0
author: Diego Nogueira
date: 2026-04-12
phases:
  - VALIDATE
  - REVIEW
---

# harness-engineering

Evaluates **harnessability** — a composite, 4-dimension metric indicating how ready the codebase is to be operated by an AI coding agent. Based on "Harness Engineering for Coding Agent Users" (Böckeler, Thoughtworks 2026).

Full guide: [docs/guides/HARNESS-ENGINEERING.md](../docs/guides/HARNESS-ENGINEERING.md)

## When to Use

- During **VALIDATE phase** — before marking a sprint or epic done, run to confirm agent-readiness did not regress
- During **REVIEW phase** — include harnessability score in code review blast radius report
- When **adding new modules** — verify type coverage and docs are in place
- After **large refactors** — fitness functions catch dependency direction violations early
- As a **periodic health check** — track score trend across releases

## Score Dimensions

| Dimension | Weight | Description |
|-----------|--------|-------------|
| Type coverage | 30% | % of public functions/classes with explicit TypeScript type annotations |
| Test coverage | 30% | Structural module→test file match (not % lines). File proximity scoring. |
| Architecture fitness | 20% | 3 fitness rules: dependency direction, no circular deps, barrel export integrity |
| Docs coverage | 20% | % of public symbols with JSDoc + presence of README/guides |

**Formula:** `score = types×0.30 + tests×0.30 + fitness×0.20 + docs×0.20`

## Grade Scale

| Grade | Score | Recommendation |
|-------|-------|----------------|
| **A** | ≥ 85 | Agent-ready. Maintain. |
| **B** | ≥ 70 | Mostly ready. Review low-score dimension (usually docs). |
| **C** | ≥ 55 | Usable but agent friction. Prioritize weakest dimension. |
| **D** | < 55 | Urgent refactor needed before expecting reliable agent operation. |

## How to Use

### 1. Run the scan

```bash
npm run harness:scan
```

Output: grade + score + breakdown per dimension + list of issues.

### 2. Interpret each dimension

**Type coverage < 70%:**
- Add return types to public functions
- Annotate exported interfaces
- Run `tsc --noEmit` to confirm type-check passes

**Test coverage < 70%:**
- Create `src/tests/<module-name>.test.ts` for uncovered modules
- Even stub tests count — the metric measures file proximity, not assertion count
- TDD: write tests BEFORE implementation (Red→Green→Refactor)

**Architecture fitness < 100%:**
- Check dependency direction: `core/` must not import `cli/`, `mcp/`, `api/`, `web/`
- Check `index.ts` barrel files — they must re-export all siblings
- Circular dependency detected? Break the cycle via interface abstraction

**Docs coverage < 70%:**
- Add JSDoc to exported functions/classes
- Ensure `README.md` exists and covers the module's purpose
- Check `docs/guides/` for user-facing documentation

### 3. Act on the grade

```
Grade A → No action. Log score in sprint review.
Grade B → Open issue for lowest dimension. Fix in next sprint.
Grade C → Block ship until ≥ 1 dimension is improved.
Grade D → Escalate. Refactor before new features.
```

### 4. Integrate with VALIDATE gate

During `analyze(mode: "validate_ready")`, check harnessability score:
- Score ≥ 70 (B) = pass for harness gate
- Score < 70 = flag as risk before REVIEW phase

## Anti-Patterns

- **Do NOT ignore D-grade** and ship anyway — agent operations will be unreliable
- **Do NOT fake test files** to boost test coverage score — structural scan checks real test logic proximity
- **Do NOT skip fitness functions** — dependency violations compound across releases
- **Do NOT treat type coverage as optional** — TypeScript strict mode + explicit types are the foundation of agent observability

## Related

- Full guide: `docs/guides/HARNESS-ENGINEERING.md`
- Scan runner: `scripts/harness-scan-run.js`
- Implementation: `src/core/harness/`
- npm script: `harness:scan`


## Codex Notes

- In Codex Plan Mode, use this skill for planning only and do not mutate files.
- During implementation, follow the project `AGENTS.md` rules and use `apply_patch` for manual edits.

---
name: graph-dependency
description: Dependency management audit using SBOM generation, license compliance, supply chain security, and freshness scoring
triggers:
  - graph-dependency
version: 1.0.0
author: Diego Nogueira
date: 2026-04-04
---

# graph-dependency

Dependency management audit using SBOM generation, license compliance, supply chain security, and freshness scoring. Identifies vulnerabilities, license risks, outdated packages, and supply chain attack vectors across all project dependencies.

## When to Use

- Before DEPLOY phase
- Monthly maintenance cycles
- When adding new dependencies
- During security reviews
- After npm audit findings

## Mandatory Flow

```
npm audit --> license scan --> freshness check --> SBOM generation --> supply chain analysis --> update plan --> report --> write_memory
```

## Workflow

### Step 1: Dependency Audit

Run `npm audit --json` for full vulnerability report. Categorize by severity (critical/high/medium/low). Check both production and dev dependencies. Compare with previous audit for trend.

- Run `npm audit --json` and parse the JSON output
- Count vulnerabilities by severity: critical, high, moderate, low
- Separate findings into production dependencies vs dev dependencies
- Check for `npm audit fix` auto-fixable issues vs manual resolution required
- Compare with previous audit memory (if exists) to identify new vs recurring vulnerabilities
- Flag any critical or high severity CVE as a blocker for DEPLOY

### Step 2: License Compliance

Check all dependency licenses via `npm ls --json`. Flag incompatible licenses: GPL in MIT project, AGPL in proprietary code, unknown licenses. Verify all production deps have OSI-approved licenses. Create allowlist/denylist.

- Run `npm ls --json --all` to get full dependency tree with license info
- Check each production dependency license against project license (MIT)
- Flag incompatible licenses: GPL-2.0, GPL-3.0, AGPL-3.0, SSPL, BSL in MIT project
- Flag unknown or missing licenses (`UNLICENSED`, `SEE LICENSE IN`, no license field)
- Verify all production deps use OSI-approved licenses
- Allowlist: MIT, ISC, BSD-2-Clause, BSD-3-Clause, Apache-2.0, 0BSD, CC0-1.0
- Denylist: GPL-2.0-only, GPL-3.0-only, AGPL-3.0, SSPL-1.0

### Step 3: Freshness Scoring

For each production dependency, check latest version vs installed version. Score: latest = 100, 1 minor behind = 80, 1 major behind = 50, >2 major = 20, unmaintained (no release in 12mo) = 0. Calculate average freshness.

- Run `npm outdated --json` to get installed vs latest version for each dependency
- Score each dependency:
  - Latest version installed = 100
  - 1 minor version behind = 80
  - 2+ minor versions behind = 60
  - 1 major version behind = 50
  - 2+ major versions behind = 20
  - No release in 12+ months (unmaintained) = 0
- Calculate average freshness score across all production dependencies
- List the bottom 10 dependencies by freshness as priority update targets
- Flag any dependency with freshness score = 0 as a risk

### Step 4: SBOM Generation

Generate Software Bill of Materials in SPDX or CycloneDX format. List all transitive dependencies. This is mandated by NIST/CISA for supply chain transparency. Output as `sbom.json` in project root.

- Use `npm sbom --sbom-format cyclonedx` (npm 10+) or equivalent tool
- Include all production and dev dependencies with versions
- Include transitive dependency tree (full depth)
- Record package name, version, license, supplier for each component
- Output as `sbom.json` in project root (CycloneDX format preferred)
- Verify SBOM completeness: total components should match `npm ls --all` count

### Step 5: Supply Chain Analysis

Check for known supply chain attack patterns: typosquatting (similar package names), dependency confusion (internal/public name collision), maintainer takeover (recent ownership changes). Flag packages with <100 weekly downloads or single maintainer.

- Check for typosquatting: compare package names against known popular packages for similar names (1-2 char differences)
- Check for dependency confusion: verify no internal package names collide with public npm registry names
- Flag packages with very low weekly downloads (<100) as potential risk
- Flag packages with a single maintainer (bus factor = 1)
- Check for recent ownership transfers or maintainer changes in the last 6 months
- Verify package integrity: `npm ls --json` should show no `extraneous` or `missing` packages
- Check `package-lock.json` integrity hashes are present for all dependencies

### Step 6: Update Plan

For each outdated dependency, assess update risk: breaking changes in changelog, test coverage for the dependency usage, migration effort. Prioritize: security fixes first, then major updates with breaking changes last. Create graph nodes for significant updates.

- Prioritize updates by category:
  1. **Critical:** Security fixes for critical/high CVEs — update immediately
  2. **High:** Security fixes for moderate CVEs — update within sprint
  3. **Medium:** Major version bumps with breaking changes — plan migration
  4. **Low:** Minor/patch version bumps — batch in maintenance window
- For each major version update, check changelog for breaking changes
- Estimate migration effort: trivial (patch), moderate (minor API changes), significant (rewrite usage)
- For significant updates, create graph nodes via `mcp__mcp-graph__node` for tracking
- Verify test coverage exists for the dependency usage before updating

### Step 7: Dependency Report

Generate the full audit report. Score 0-100 (audit 30%, licenses 20%, freshness 25%, supply chain 25%). Breakdown by category. Save via `mcp__mcp-graph__write_memory`.

```
Tool: mcp__mcp-graph__write_memory
Params:
  title: "Dependency Audit — <date>"
  content: "<findings summary with vulnerability counts, license issues, freshness score, supply chain risks>"
  tags: ["dependencies", "audit", "security", "sbom", "supply-chain"]
```

**Grading:**
- **A (90-100):** Zero critical/high CVEs, all licenses compliant, freshness > 80%, no supply chain risks
- **B (75-89):** No critical CVEs, minor license issues, freshness > 65%, low supply chain risk
- **C (60-74):** Some high CVEs with fix available, license warnings, freshness > 50%
- **D (45-59):** Critical CVEs present, license violations, freshness < 50%, supply chain concerns
- **F (< 45):** Multiple critical CVEs unfixed, license blockers, widespread outdated deps, active supply chain risks

## Output Format

```
Phase: DEPENDENCY AUDIT
Vulnerabilities: <N> critical, <N> high, <N> moderate, <N> low
License Issues: <N> incompatible, <N> unknown
Average Freshness: <N>%
SBOM: generated (<N> components)
Supply Chain Risks: <N> findings
Update Plan: <N> critical, <N> high, <N> medium, <N> low priority
Overall Grade: <A-F>
Recommendations: <top 3 actions>

Saved to memory: "Dependency Audit — <date>"
```

## Anti-Patterns

- Do NOT add dependencies without checking license compatibility — one GPL dep can relicense your project
- Do NOT ignore major version bumps — they have breaking changes that must be assessed
- Do NOT skip dev dependency audit — build tools can be compromised too
- Do NOT trust downloads count alone — check maintainer reputation and package history
- Do NOT delay security updates — critical CVEs need immediate action
- Do NOT run npm install without reviewing what changed in package-lock.json — supply chain attacks hide there


## Codex Notes

- In Codex Plan Mode, use this skill for planning only and do not mutate files.
- During implementation, follow the project `AGENTS.md` rules and use `apply_patch` for manual edits.

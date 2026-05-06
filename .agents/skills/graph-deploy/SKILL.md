---
name: graph-deploy
description: Execute the DEPLOY phase of mcp-graph lifecycle — DORA release health, deploy_ready 7 checks, CI pipeline, post-release validation
triggers:
  - graph-deploy
version: 2.0.0
author: Diego Nogueira
date: 2026-04-04
---

# graph-deploy

Execute the DEPLOY phase of the mcp-graph lifecycle. Handles CI pipeline execution, release validation via DORA metrics, and post-release verification with deploy_ready gate (7 checks).

## When to Use

- After HANDOFF phase has created the PR and documentation
- Running CI pipeline and verifying green builds
- Creating a release or version tag
- The `_lifecycle.phase` returned by mcp-graph is `DEPLOY`

## Mandatory Flow

```
forecast(dora) → analyze(release_check) → [CI/merge] → snapshot → analyze(deploy_ready) → set_phase(LISTENING)
```

## Workflow

### Step 1: DORA Release Health Assessment

```
Tool: mcp__mcp-graph__forecast (mode: "dora")
```

Review before deploying:
- **Deployment Frequency** — tasks/day (target: > 2/day for Elite)
- **Lead Time** — P85 hours created→done (target: < 24h for Elite)
- **Change Failure Rate** — status reversals / done (target: < 5%)
- **MTTR** — mean time to recover from rework (target: < 1h)
- **Trend** — improving / stable / declining

If trend is `declining` or change failure rate > 10%, flag risk before proceeding.

### Step 2: Release Check

```
Tool: mcp__mcp-graph__analyze (mode: "release_check")
```

Verifies:
- Semantic versioning consistency
- CHANGELOG updated
- CI status green
- All sprint tasks done
- No critical issues open
- Documentation complete

### Step 3: CI Pipeline

Monitor CI status:
```bash
gh pr checks <PR_NUMBER>
gh run list --limit 5
```

Wait for all checks to pass. If checks fail: diagnose, fix, push, re-run.

### Step 4: Merge

Once CI is green:
```bash
gh pr merge <PR_NUMBER> --squash
```

Or ask user for merge strategy preference (squash, merge, rebase).

### Step 5: Post-Release Validation

After merge, verify:
```bash
git pull origin master
npx vitest run
```

Ensure the merged code works on the target branch.

### Step 6: Create Snapshot (CFD tracking)

```
Tool: mcp__mcp-graph__snapshot
```

Records post-deploy state. This feeds the Cumulative Flow Diagram for flow analysis.

### Step 7: Version Tag (if applicable)

If this is a release milestone:
```bash
git tag -a v<version> -m "Release v<version>"
```

### Step 8: Validate Deploy Gate (7 checks)

```
Tool: mcp__mcp-graph__analyze (mode: "deploy_ready")
```

**Gate checks (5 required + 2 recommended):**

| # | Check | Severity |
|---|-------|----------|
| 1 | CI passed | **required** |
| 2 | PR merged | **required** |
| 3 | Post-release tests pass | **required** |
| 4 | Snapshot created | **required** |
| 5 | All sprint tasks done | **required** |
| 6 | Version tagged | recommended |
| 7 | DORA metrics healthy | recommended |

### Step 9: Transition

Once gate passes:
```
Tool: mcp__mcp-graph__set_phase (phase: "LISTENING")
```

Follow `_lifecycle.nextAction` for the recommended next tool call.

## Output Format

```
Phase: DEPLOY → LISTENING
CI: green
PR: #N merged
Tag: v<version> (if applicable)
DORA: freq X/day, lead P85 Yh, failure Z%, MTTR Wh, trend T
Gate: deploy_ready — score N/100, grade X
Status: Ready to proceed to LISTENING phase
```

## Anti-Patterns

- Do NOT merge with failing CI — fix first
- Do NOT skip post-release validation — merge conflicts can introduce bugs
- Do NOT force push to main/master — ever
- Do NOT delete branches before confirming merge succeeded
- Do NOT deploy with declining DORA trend without team awareness
- Do NOT ignore `_lifecycle.nextAction` — it guides the optimal next action
- Do NOT skip snapshot — it feeds CFD flow analysis for future sprints


## Codex Notes

- In Codex Plan Mode, use this skill for planning only and do not mutate files.
- During implementation, follow the project `AGENTS.md` rules and use `apply_patch` for manual edits.

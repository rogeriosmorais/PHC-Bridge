# Stage 1 Handoff Format

## Purpose

Every AI-owned Stage 1 task must end in the same handoff shape so another engineer, agent, or the user can pick it up without re-reading the entire planning thread.

## Required Sections

Use this exact section order:

1. `Task ID`
2. `Decision summary`
3. `Produced artifact`
4. `What changed`
5. `Dependencies satisfied`
6. `Open risks`
7. `Blocked by user?`
8. `Verification`
9. `Next consumer`

## Markdown Template

```md
# <Task ID> Handoff

## Task ID
<task id>

## Decision Summary
- <high-signal planning or implementation decisions only>

## Produced Artifact
- <absolute path or repo-relative path>

## What Changed
- <documents, code, tests, or evidence produced>

## Dependencies Satisfied
- <upstream tasks, assumptions, or prerequisites now satisfied>

## Open Risks
- <remaining technical or process risks>

## Blocked By User?
- yes/no
- If yes: <exact user action needed>

## Verification
- <tests run, static review performed, or manual evidence collected>
- If manual verification is required: link the ELI5 checklist

## Next Consumer
- <task id or human role that should pick this up next>
```

## Artifact Rules

- If a task creates a planning document, the handoff must link it directly.
- If a task creates evidence for a gate, the handoff must state whether the gate is currently pass, fail, or blocked.
- If a task depends on user action, the handoff must name the exact evidence expected back from the user.

## Review Rules

- Handoffs should be short enough to scan quickly.
- Do not repeat the entire task history.
- Do not hide blockers inside prose. Put them in `Blocked By User?`.

# Review Reports

This folder stores durable review evidence for implementation task packets.

A task status may not change to:
- `fix-required`
- `rejected`
- `accepted`

unless there is a review report or inline reviewer output containing the required verdict evidence.

## Required Review Report Fields

Each review report must include:

- task ID
- task packet path
- task base SHA
- task head SHA
- commit SHA
- blockers
- non-blocking nits
- verdict
- next action

## Valid Verdict Rules

- `accept` requires blockers = `none`.
- `fix required` requires at least one blocker.
- `reject` requires at least one blocker.

A bare verdict without blocker details is invalid.

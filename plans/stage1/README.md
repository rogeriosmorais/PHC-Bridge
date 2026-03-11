# Stage 1 Planning Bundle

This folder is organized by role, not by chronology.

Use these subfolders:

- `00-control`
  Control docs that define how Stage 1 is orchestrated and handed off.
- `10-specs`
  Frozen technical specs and lock documents.
- `20-execution`
  Live execution packages, ledgers, logs, and runbooks.
- `30-evidence`
  Gate evidence and evaluation verdict documents.
- `40-tasks`
  Task graph and task packets.
- `50-content`
  Content, motion, checkpoint, and scaffold locks.
- `60-user`
  Human-facing runbooks, checklists, and return templates.

Source-of-truth rules:

- [`STAGE1_PLAN.md`](/F:/NewEngine/STAGE1_PLAN.md) is the Stage 1 index and control document.
- Files in `10-specs` define frozen contracts unless an explicit later execution document says otherwise.
- Files in `20-execution` record the active execution state.
- Files in `30-evidence` record gate outcomes, not implementation intent.
- Files in `60-user` should remain operator-facing and easy to follow.

Reorganization rule:

- Prefer moving a file only when its role has changed.
- Prefer updating links in `STAGE1_PLAN.md` rather than duplicating summary content.

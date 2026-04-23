# Stage 1 Assumption Ledger

## Purpose

This is the orchestrator-owned control document for Stage 1. It tracks the assumptions that can kill the project, the signals that would prove them wrong, and the immediate decision for each.

## Status Scale

- `green`: acceptable for downstream work
- `yellow`: plausible, but under active watch
- `red`: likely false; downstream work should stop

## How To Update This Ledger

Update this ledger ONLY when one of these events occurs:
1. A new planning document is accepted.
2. A test slice is defined.
3. A test slice goes **RED** or **GREEN**.
4. A fallback becomes more likely than the primary path.
5. An assumption changes status.
6. A blocked task becomes unblocked or newly blocked.

**When updating**: Change statuses, blocked tasks, falsification signals, or immediate fallbacks directly. **Do not append historical prose.**

## Critical Assumptions (Rewrite Frontier)

| ID | Assumption | Why It Matters Now | Current Status | Next Test | Falsification Signal | Immediate Fallback | Stop Work If False | Blocked Tasks | Last Reviewed |
|---|---|---|---|---|---|---|---|---|---|
| **A-01** | Internal contract coherence | Avoids implementation-time dead ends | `green` | `INTEG-01` | Contradictions emerge during TDD | Revise specialized contracts | **Yes** | `S1-IMPL-BALANCE-FIRST` | 2026-04-23 |
| **A-02** | Pure support-truth extraction | Foundation for implementation integrity | `green` | `LOGIC-01` to `LOGIC-04` | Logic cannot be decoupled from Unreal | Adapter layer / postpone surgery | **Yes** | `S1-IMPL-BALANCE-FIRST` | 2026-04-23 |
| **A-03** | Adapter-fed validators | Keeps truth logic testable and portable | `green` | `VALID-01` to `VALID-07` | Validators require deep Unreal wiring | Implement as thin Unreal wrappers | **Yes** | `S1-IMPL-BALANCE-FIRST` | 2026-04-23 |
| **A-04** | TDD slice-by-slice progression | Prevents broad, high-risk refactors | `green` | Slice 1 acceptance | Slices require multi-surface changes | Widen slice scope / reassess | **Yes** | `S1-IMPL-BALANCE-FIRST` | 2026-04-23 |
| **A-05** | Artifact-only reconstruction | Ensures forensic truth without logs | `green` | `LOGIC-05` artifact audit | Failures require log forensics | Expand artifact schema | **Yes** | `S1-IMPL-BALANCE-FIRST` | 2026-04-23 |
| **A-06** | Late state-machine rewiring | Minimizes surgery on active runtime | `green` | `INTEG-01` wiring | State machine depends on unbuilt logic | Pull rewiring earlier | **Yes** | `S1-IMPL-BALANCE-FIRST` | 2026-04-23 |
| **A-07** | 3.0s standing is V0 target | Definitive product success gate | `green` | `INTEG-07` | Target found to be unreachable | Revise benchmark | No | None | 2026-04-23 |
| **A-08** | Sufficient data access | Validators must be fed truthfully | `green` | Slice 1 implementation | Required Unreal data is inaccessible | Modify bridge component | **Yes** | `S1-IMPL-BALANCE-FIRST` | 2026-04-23 |
| **A-09** | Pure-first implementation | Ensures TDD discipline at startup | `green` | Slice 1 unit suite | Slice 1 requires runtime surgery | Insert adapter stubs | **Yes** | `S1-IMPL-BALANCE-FIRST` | 2026-04-23 |
| **A-10** | Pure-logic slice viability | Support truth logic can be decoupled from Unreal | `green` | Slice 1 unit suite | Logic requires direct engine object access | Introduce adapter boundary | **Yes** | `S1-IMPL-BALANCE-FIRST` | 2026-04-23 |
| **A-11** | Validator-first refactor order | Prevents surgery before tests exist | `green` | `VALID-01` to `VALID-07` | Runtime edits necessary before tests are green | Insert narrower extraction layer | **Yes** | `S1-IMPL-BALANCE-FIRST` | 2026-04-23 |

## Resolved / Locomotion-Era (Historical)

- `A-LOCO-01` (Training): Policy looks alive enough to justify integration.
- `A-LOCO-02` (Contract): PHC config can be mapped into the bridge contract.
- `A-LOCO-03` (Retargeting): SMPL-to-Manny transforms are stable enough.
- `A-LOCO-04` (PhysicsControl): Expresses policy intent well enough for POC.
- `A-LOCO-05` (Chaos): Substepping at 120-240 Hz is stable enough.
- `A-LOCO-06` (NNE): Model load and inference are compatible.

## Reassessment Triggers

- A planning spec is locked.
- A gate package is prepared.
- User evidence arrives.
- Implementation spike prove a contract is physically impossible.

## Planning Authority

The following documents are the ONLY active authority for Stage 1 execution:
- **Contract Suite**: `plans/stage1/10-specs/*.md`
- **TDD Planning**: `plans/stage1/20-execution/balance_first_*` and `plans/stage1/40-tasks/*`
- **History**: `plans/stage1/20-execution/execution-log.md`

**Deprecated/Purged**:
- `bridge-spec.md`, `retargeting-spec.md`, `test-strategy.md`
- Archived `40-design` design artifacts
- Phase 0 machine-specific execution packages

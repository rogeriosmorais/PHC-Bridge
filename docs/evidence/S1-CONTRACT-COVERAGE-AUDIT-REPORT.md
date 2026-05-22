# S1 Contract Coverage Audit Report

## Classification of Active Contracts

| Contract ID | Title | Classification | Rationale |
|-------------|-------|----------------|-----------|
| `node_828bafe36d66` | Contract: Authority Matrix | runtime truth gate | Judges structural authority during proof window. |
| `node_afc6c72a4054` | Contract: Balance Mode Entry | pre-entry gate | Defines states and entry criteria. |
| `node_cf01194d0393` | Contract: Character Capsule | pre-entry gate | Ensures capsule isolation and root lock. |
| `node_29238c1ca41e` | Contract: Continuous Balance Architecture | pre-entry gate | Defines V0 scope and truth set. |
| `node_b895d7c254ff` | Contract: Continuous Balance Truth Model | runtime truth gate | Judges raw body continuity and terminal precedence. |
| `node_d6efdfd0812c` | Contract: Engine Execution | runtime truth gate | Ensures cadence and execution order truth. |
| `node_c6e2c62a4bc8` | Contract: Instrumentation and Acceptance | acceptance/artifact gate | Defines schema and product success thresholds. |
| `node_b2b00018cb16` | Contract: Physics Asset | pre-entry gate | Ensures structural audit and geometry truth. |
| `node_fbd95aec7ed4` | Contract Gate: First BalanceActive Product Success | acceptance/artifact gate | Final aggregation gate for product success. |

## Superseded / Duplicate Contracts

The following generic contracts are superseded by the authoritative detailed nodes above:
- `node_9620b347a76d` (Authority Contract) -> Superseded by Authority Matrix (`node_828bafe36d66`).
- `node_63f1001445c4` (Balance Contract) -> Superseded by Truth Model (`node_b895d7c254ff`) and Instrumentation (`node_c6e2c62a4bc8`).
- `node_7cc0fdad69d6` (Capsule Contract) -> Superseded by Character Capsule (`node_cf01194d0393`).
- `node_7afb37fec6f7` (Continuity Contract) -> Superseded by Truth Model (`node_b895d7c254ff`).
- `node_e4222cd896b5` (Log Contract) -> Superseded by Instrumentation and Acceptance (`node_c6e2c62a4bc8`).

## Later Stage (G2/G3) Contracts

The following contracts are classified as not relevant for the first BalanceActive V0 claim:
- `node_ece84879768c` (Control Contract) -> Later G2/G3 context.
- `node_bc4c6d92e40d` (Floor Contract) -> Later G2/G3 context.
- `node_175ed4f18c25` (Model Contract) -> Later G2/G3 context.
- `node_3f8865a0a700` (Search Contract) -> Later G2/G3 context.

## Workflow Edge and AC Audit

| Contract | Has AC? | Has Workflow Edge? | Action |
|----------|---------|--------------------|--------|
| Authority Matrix | YES | YES | None |
| Balance Mode Entry | YES | **NO** | Add `depends_on` from `node_fe74d8d94e21` |
| Character Capsule | YES | YES | None |
| Architecture | YES | YES | None |
| Truth Model | YES | YES | None |
| Engine Execution | YES | YES | None |
| Instrumentation | YES | YES | None |
| Physics Asset | YES | YES | None |
| Product Success Gate | YES | YES | None |

## Conclusion
The audit is complete. One missing workflow edge was identified for "Balance Mode Entry". Generic contracts are correctly linked as `related_to` their authoritative counterparts.

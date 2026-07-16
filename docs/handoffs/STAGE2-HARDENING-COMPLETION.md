# Stage 2 Hardening Completion

Date: 2026-07-16
Implementation branch: `parallel/complete-hardening`
Evidence source commit: `68f86ffe8807424313bc2004021c9c28066cdce7`
Base branch: `mcp-graph`

## Remote preservation

Before completing the remaining work, the committed `mcp-graph` history at `c4d3f22dfb8f6d8883eea7a3deb9725ab743a117` was pushed to `origin/mcp-graph`. An immutable rollback branch was also pushed as `origin/backup/mcp-graph-c4d3f22-20260716`.

The completion work was developed in an isolated worktree and continuously pushed to `origin/parallel/complete-hardening` so unrelated uncommitted main-agent work in the primary worktree was never staged, reset, or cleaned.

## Completed tracks

### Policy tensor and frame parity

`scripts/validate_policy_tensor_parity.py` now independently rebuilds the UE runtime's `self_observation`, `mimic_target_poses`, and `terrain` tensors from a provenance artifact. Synthetic regression cases cover forward movement, lateral movement, and a 30-degree turn.

A clean UE 5.7 product fixture captured authoritative provenance and buffers. The retained parity report records zero mismatches across all 7,109 input values at tolerance `1e-5`; the worst absolute error was below `5e-8`.

### Causal locomotion evaluator

`product-gates/scripted-locomotion.v3.json` is an immutable successor to v2. It preregisters independently interpretable physical-root gates for projected progress, root-to-shell progress ratio, lateral divergence, average tracking error, final tracking error, maximum tracking error, classification, and forbidden assistance.

`evaluate_scripted_locomotion_protocol.py --evaluate-causal` applies those gates only after protocol and evidence linkage validation. Historical v1/v2 evidence remains unchanged and receives `NOT_APPLICABLE` rather than retroactive evaluation.

### Determinism and environment reproducibility

`audit_locomotion_determinism.py` distinguishes malformed authority (`INVALID`) from repeatable behavioral failure (`FAIL`). The v3 cross-process same-machine campaign requires three clean runs with matching commit, model, normalized protocol hash, and environment authority digest; byte-identical policy input snapshots; and bounded causal-endpoint spread.

Three clean UE 5.7 processes completed with fixture verdict `PASS`. Their authority matched, input snapshots were byte-identical, and every preregistered numerical endpoint had zero spread. The campaign verdict is nevertheless `FAIL`, because all three runs consistently violated the v3 causal tracking gates. This is reproducible physical-root tracking failure, not nondeterminism.

### Unreal orchestration authority

The selected protocol path now flows from the orchestration script through `build.ps1` into the Unreal fixture. Protocol hashes are line-ending independent across C++ and Python. Child exit codes are persisted explicitly to avoid PowerShell redirected-process races. Successful Unreal tests with runtime warnings are accepted as completed fixtures while behavioral warning interpretation remains outside the fixture validator. Provenance capture can be requested through the hardened runner and is published for locomotion fixtures.

### Graph and architecture cleanup

Architecture contracts and handoffs were already integrated. The final graph cleanup made only evidence-backed changes:

- `E80 promote query-trajectory future-root placement` was changed from `ready` to `blocked`: implementation exists, but its required v2 authoritative probe is `INVALID` because `locomotion-frame-replay.json` is missing, and the run fail-stops with 119 script-step failures.
- The standing soak, G2 presentation package, G2 baseline refresh, G3 readiness package, G3 go/no-go decision, and G3 showcase scope nodes were changed from `done` to `blocked`. Current repository evidence states that the earlier three-second standing result was invalid because the pelvis was not simulating and that the current route still ends in `BalanceSafeDeny/phase3_post_root_on_instability`.
- A conflicting secondary parent/child pair for `EVIDENCE: Thigh-Hip Isolation Results` was removed while preserving its authoritative parent and the semantic `provides` relationship.

After these corrections:

- done-integrity analysis passes;
- orphan-task inference reports no implemented-but-open candidates;
- status-flow compliance is 100 percent;
- graph health reports zero critical issues.

The graph still contains 117 legacy warnings: old standalone task nodes, stale backlog records, missing reciprocal parent edges, and redundant dependency/block edge pairs. They were not bulk-deleted or fabricated into completion because their historical ownership and intended hierarchy require a separate migration review.

## Validation and evidence

- Python gate: `190 passed` using `python -m pytest scripts/tests Training/tests/test_policy_tensor_reference.py -q`.
- UE 5.7 editor compilation: passed.
- Focused Unreal protocol-authority test: passed.
- Three separate-process v3 Normal fixture runs: passed fixture validation.
- UE 5.8 editor remained running and was neither blocked nor terminated.
- Durable evidence: `docs/evidence/stage2-hardening-completion/`.

## Product conclusion

The hardening work is complete, but it does not claim locomotion product acceptance. The new evaluator exposes a deterministic, repeatable causal tracking failure: the physical root overshoots and diverges laterally from the scripted shell with large sustained tracking error. Future locomotion implementation should address that failure without weakening v3 or reclassifying the retained evidence.

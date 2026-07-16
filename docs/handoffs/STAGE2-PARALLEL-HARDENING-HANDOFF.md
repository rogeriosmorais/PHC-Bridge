# Stage 2 Parallel Hardening Handoff

Baseline used by all parallel branches: `409f8971e250212e3d8ccadbee2c675a1b82ee89`.

The main runtime branch was not modified. Each track lives in an isolated worktree and can be reviewed or cherry-picked independently.

## Commits

| Branch | Commit | Purpose |
|---|---|---|
| `parallel/policy-contract` | `eb08dfffe4402886a4fab85d5cc9fe95559c9808` | Machine-readable PHC tensor/frame contract, pure Python reference builder, synthetic layout/frame tests |
| `parallel/asset-audit` | `2d41e3f13a73883de70e8ef2e2305a1dc62cbb43` | Dedicated Unreal audit for all 16 walk/jog root-motion assets |
| `parallel/asset-audit` | `c03f93c59dfaec65c99100c7aa7026d59cf579b3` | Durable automation success report for the corpus audit |
| `parallel/evidence-audit` | `8acd78ce09620e504b74090d30d814fdf33a7dc5` | Experiment/result/manifest/artifact/protocol consistency scanner |
| `parallel/evidence-audit` | `1db23c289826bca1cb92230e8203c35c246ae393` | Adversarial causal locomotion endpoint library and bundle-membership checks |
| `parallel/evidence-audit` | `a83b8b4a6b0474a32c47e8f9240f3f8efdd2aa82` | Cross-platform locked-policy hashing; normalizes line endings before digest verification |
| `parallel/run-orchestration` | `99898c82d42e61d5a614df119ef873fbd766fe9c` | Single-process Unreal episode runner and fixture validator |
| `parallel/run-orchestration` | `315bce679326c44843ef6eb3e20a7774cc6f024f` | Authority/environment fingerprint and determinism-level contract |
| `parallel/stage2-docs` | `b62a2ec5f7dc962d2079467ffb6bcc8139fffee4` | Stage 2A claim, authority, frame glossary, and architecture diagrams |

## Validation completed

- Policy tensor reference: 4 Python tests passed.
- Evidence/protocol audit: 3 Python tests passed.
- Adversarial causal metrics: 7 Python tests passed.
- UE automation fixture validator: 4 Python tests passed.
- Environment fingerprint: 3 Python tests passed.
- Stage 2 documentation contract: 3 Python tests passed.
- Repository fast Python tier after the line-ending fix: 138 tests passed.
- Pose Search corpus audit: Unreal compile succeeded; `PhysAnim.Development.PoseSearchCorpusAudit` succeeded with zero warnings and zero errors.
- All 16 expected movement assets loaded and satisfied their locked direction contracts.

## Important findings

1. The current locked `scripted-locomotion.v1.json` schedule does not match the runtime phase constants introduced after E69. The evidence audit records this as `locked_protocol_schedule_mismatch`.
2. The current runtime tensor packing is now independently documented and executable, but exact parity with the missing original ProtoMotions checkpoint preprocessing is still not proven.
3. The full unarmed locomotion corpus confirms authored forward `+Y`, authored left `+X`, and approximately 300 cm/s walk roots; jog roots range approximately 500–600 cm/s.
4. Causal endpoints now expose statue behavior, reversed progress, lateral divergence, forbidden assistance, tracking error over time, and selective/duplicated bundle membership without defining protocol-v2 thresholds.
5. The orchestration wrapper avoids the observed Boolean-string and bridge-timeout relaunch failures by generating a typed child invocation and polling the same process.
6. The locked impact-policy digest matched LF bytes but failed on CRLF checkouts. The fix canonicalizes only line endings before hashing, preserving the locked logical content and restoring the fast test tier on Windows.

## Integration order

Recommended low-conflict order:

1. `eb08dff` — policy contract and independent builder.
2. `2d41e3f`, then `c03f93c` — corpus audit and durable report.
3. `8acd78c`, then `1db23c2`, then `a83b8b4` — evidence audit, causal metrics, and cross-platform locked-policy hashing.
4. `99898c8`, then `315bce6` — runner, validator, and fingerprint.
5. `b62a2ec` — architecture and Stage 2A claim documentation.

Before cherry-picking, compare each commit against the main agent's protocol-v2 and frame-adapter work. The parallel branches deliberately avoid the main runtime files, but the documentation and orchestration contracts may need terminology updates after protocol v2 is finalized.

After integration:

- rerun all included Python tests;
- rerun `PhysAnim.Development.PoseSearchCorpusAudit`;
- rerun the experiment evidence audit against the integrated tree and retained run artifacts;
- update/reindex graph and code intelligence;
- keep product protocol, runtime behavior, evidence, and documentation changes in separate commits.

## MCP graph changes

- Pre-change snapshot: `288`.
- New reusable template: `PHC Stage2A Causal Tracking`, template node `node_eb579e679374`.
- The template was applied under the existing Stage 2 parent, producing a compact nine-task critical path.
- Integration task: `node_dd64c98ec638`, status `ready`, with all branch and commit references.

The historical graph still contains unrelated legacy integrity issues. The new Stage 2 template does not attempt to rewrite or falsely close those records.

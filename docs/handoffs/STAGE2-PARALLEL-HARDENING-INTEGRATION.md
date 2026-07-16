# Stage 2 Parallel Hardening Integration

Date: 2026-07-16
Target branch: `mcp-graph`

## Result

The isolated Stage 2 hardening work is integrated into `mcp-graph`.

- Pre-integration safety point: `3cbaa52761d6818ad0eb9d428569c27671fde1bf`
- Backup branch: `backup/mcp-graph-before-parallel-merge-20260716`
- Parallel ancestry merge: `c62425af539f1bf4687509ad63767da2a0392b4e`
- Orchestration/process-safety reconciliation: `78695f6950ef3c03a2027910dd2ddf2eb591fc71`

The merge commit records all original branch heads as parents, so Git reports every `parallel/*` branch as merged.

## Integrated branches

| Branch | Original head |
|---|---|
| `parallel/policy-contract` | `eb08dfffe4402886a4fab85d5cc9fe95559c9808` |
| `parallel/asset-audit` | `c03f93c59dfaec65c99100c7aa7026d59cf579b3` |
| `parallel/evidence-audit` | `737fe4bfae2142ec4dbfaf4be1a311ef90868b6a` |
| `parallel/run-orchestration` | `71057c5ee09fb662c98452ac7d241e3a04067864` |
| `parallel/stage2-docs` | `e6343eaaae4ae30725de331b8d13917a0ef8ecdc` |
| `parallel/integration-validation` | `4301a63a5cd5849b233ecfb92a4137827cc1ad1f` |

## Applied content commits on `mcp-graph`

- Policy contract: `a157a83`
- Pose Search corpus audit: `6142474`, `727461d`
- Evidence and causal metrics: `d69bb10`, `6b0966f`, `4259688`, `a8ed4fd`
- Unreal orchestration and fingerprinting: `09dd7ec`, `2e03daf`, `ba7d0c6`
- Stage 2 architecture and handoff documentation: `2102ec9`, `338a47b`, `40cf409`, `8f32ebe`

Concurrent E79 work already present on `mcp-graph` was retained without alteration: `e853d11` and `cce8faa`.

## Integration resolution

The orchestration branch originally contained an unscoped `taskkill /T` timeout path. That conflicted with the repository's UE 5.7 process-safety contract. The integrated implementation now:

- validates `UE5_PATH` as Unreal Engine 5.7;
- ignores UE 5.8 editors during duplicate-process checks;
- centralizes all process termination in `UnrealProcessSafety.psm1`;
- validates every Unreal descendant in a captured timeout process tree before terminating anything;
- refuses cleanup without terminating the tree if a UE 5.8, foreign-engine, or path-unverifiable Unreal process is present;
- terminates validated child trees deepest-child-first.

## Validation

- `python -m pytest scripts/tests Training/tests/test_policy_tensor_reference.py -q`: **167 passed**.
- UE automation orchestration dry-run against `scripted-locomotion.v2.json`: **passed**.
- UE 5.7 editor build: **succeeded**.
- `PhysAnim.Development.PoseSearchCorpusAudit`: **succeeded** with exit code 0.
- `git diff --check`: **passed**.
- All six `parallel/*` branch heads appear in `git branch --merged mcp-graph`.

The complete `Training/tests` collection was not used as the integration gate because this checkout lacks optional `protomotions` and `onnx` Python dependencies. The newly merged policy-tensor reference test was run explicitly.

## Evidence audit state

A post-merge audit completed successfully as a scanner, but returned the expected failing repository verdict:

- 81 blocking findings;
- 11 warnings;
- 69 historical dirty-source run records;
- 11 preregistrations without results;
- 11 legacy result records without matching preregistrations;
- one locked v1 schedule mismatch.

The generated local audit output was kept under `_tmp` rather than replacing the tracked baseline report, because the scan included locally present untracked historical `test-results` trees.

## Repository state

No product-acceptance claim is made by this merge. The integration adds methodology, contracts, audits, orchestration, reproducibility tooling, documentation, and regression coverage. Existing untracked `test-results` directories were not staged or deleted.

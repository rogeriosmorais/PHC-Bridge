# Current Product State

## Authoritative Product Status: **BLOCKED**

- V2 signed receipt: **ABSENT**
- V2 trusted public-key fingerprint: **NOT PROVISIONED**
- Protected objective: `node_ee2e75c0b78d` (**BLOCKED**)
- Gate: `standing-v0` contract v2
- Contract SHA-256: `ce3e094c6e1731eb26222bb17c70dd82db76561c43f1fec3daed518c140cf32d`
- Issuer: `physanim-product-oracle-v2`

The v1 objective remains blocked and retired from the v2 authority path. Local
automation results, graph status, reports, diagnostic classifications, and node
counts cannot change the product status.

## Verified Boundary Mechanics

As of 2026-07-10, deterministic tests verify these infrastructure properties:

- The implementation repository cannot call or parameterize the signer.
- Local diagnostics use a byte-identical v2 contract reference and production-
  shaped `setup_override_count=null`; they no longer inject a fake zero or use
  v1 thresholds to classify v2 evidence.
- The external oracle has no arbitrary evidence signing endpoint. It accepts an
  MCP challenge and runs one fixed renderer-enabled product attempt after the
  required clean build/test gate.
- The v2 oracle evaluates canonical raw envelope bytes and hashes those exact
  bytes into a short-lived Ed25519 receipt. Negative measurements, single-sample
  duration claims, caller-authored verdicts, and threshold overrides fail.
- The protected runner removes the signing key and `NODE_OPTIONS` before any
  repository, Git, build, or UE process executes. It binds the clean source,
  fixed command/test, UE executable, target receipt, ONNX/UAsset models, and one
  nonce-matching runtime artifact.
- The guarded `F:\GlobalMCP2` MCP stores immutable bindings, challenges,
  evidence, trust anchors, receipts, and one-shot completion contexts. It checks
  the exact envelope digest and rejects direct, stale, dirty, replayed, or
  unselected completion attempts.
- The live workflow database has v1 and v2 protected bindings, migration v3,
  zero accepted receipts, zero consumptions, and zero trust anchors.

These checks verify enforcement mechanics. They do not prove humanoid behavior.

## Not Verified

- The humanoid has not passed a fresh protected `standing-v0` v2 run.
- No authoritative attempt currently proves PoseSearch selection, real NNE
  policy inference, nontrivial Physics Control work, Chaos body continuity,
  support behavior, and rendered motion continuously for the required window.
- The production signing key and append-only public-key fingerprint have not
  been provisioned by a separate owner.
- Oracle and MCP files on this machine are writable by the same Windows user.
  Local ACLs and SQLite triggers are defense in depth, not an independent
  principal. An administrator or same-user process can replace them.
- Runtime facts are still emitted by implementation code. Fixed execution,
  artifact selection, binary/model hashes, and independent thresholds make
  fabrication harder, but they are not an independent physical sensor.
- The proxy checks source immediately before and after child `finish_task` and
  blocks on drift. SQLite cannot inspect Git at the exact internal status-write
  instruction, so a narrow local TOCTOU window remains without a child-runtime
  callback or external finalizer.
- The on-disk project configuration points to the guarded `F:\GlobalMCP2`
  proxy, but already-running host MCP processes were launched through the old
  discontinued `npx` path. A host restart is required before the new tool
  surface is active in every session. The live SQLite triggers are already
  installed and protect both objective nodes meanwhile.

## Historical Diagnostic

The 2026-06-13 attempt `2266F239-416B-020D-5616-6FB7C055263F` was contradictory:
automation logs claimed success while structured artifacts reported blocked
support evidence. It is a historical debugging snapshot, not product evidence.

## Unblock Conditions

1. A separate service owner protects the v2 oracle/runtime and private key, then
   provisions the matching Ed25519 SPKI fingerprint and public key to MCP.
2. The v2 objective enters `in_progress` and guarded MCP issues a fresh challenge
   for one clean committed source tree.
3. The protected service performs the fixed build and renderer-enabled run,
   evaluates the exact artifact envelope against frozen v2, and signs PASS.
4. Guarded MCP verifies the key fingerprint, receipt, exact envelope, challenge,
   attempt, source, and task start, then consumes that selected receipt during
   protected completion with no source drift.

Until all four conditions occur, the honest product status is **BLOCKED**.

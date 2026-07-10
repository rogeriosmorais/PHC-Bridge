# Current Product State

## Authoritative Product Status: **BLOCKED**

- Signed receipt: **ABSENT**
- Trusted `physanim-ci` public key: **NOT PROVISIONED**
- Gate: `standing-v0` contract v1
- Contract SHA-256: `44e3a23e43c854cbac9509a948826f499f7125dbd9d1dc1e8059179ed12c5bee`
- Authority: protected external oracle only

Local automation results, graph status, reports, and runtime-authored classifications cannot change this status.

## Verified Boundary Mechanics

As of 2026-07-10, deterministic tests verify these infrastructure properties:

- The project and external oracle contain byte-identical locked contract bytes.
- The local evidence tools identify themselves as diagnostic-only and cannot emit a product receipt.
- The external oracle rejects implementation-authored result fields, changed thresholds, stale evidence, wrong run nonces, wrong commits, malformed JSON, and missing signing authority.
- The guarded `F:\GlobalMCP2` MCP runtime verifies Ed25519 receipts against an external trust root and rejects direct completion of the protected objective without one.
- The live workflow database has one protected objective binding and no accepted or consumed product receipts.

These checks verify the boundary. They do not verify humanoid behavior.

## Not Verified

- The humanoid has not passed a fresh externally controlled `standing-v0` run.
- No authoritative run currently proves PoseSearch selection, real NNE policy inference, Physics Control target writes, Chaos body continuity, support behavior, and rendered motion in one continuous attempt.
- The production signing key and public trust root have not been provisioned by a separate owner.
- The local MCP database and oracle files are not an independent security principal. A same-user process with filesystem and database administration rights can still replace local enforcement. Operational authority therefore requires protected CI or a signing service outside the implementation agent's credentials.

## Historical Diagnostic

The 2026-06-13 attempt `2266F239-416B-020D-5616-6FB7C055263F` was contradictory: automation logs claimed success while structured artifacts reported blocked support evidence. It is a historical debugging snapshot, not current product evidence and not a basis for planning counts or completion claims.

## Unblock Conditions

1. A separate oracle owner provisions the `physanim-ci` public key to the MCP trust store while keeping the private key outside the project, MCP checkout, and oracle checkout.
2. A clean committed source revision is built and exercised by a protected runner with a fresh nonce and a fixed standing test command.
3. The external oracle evaluates the fresh raw facts against the locked contract and signs a short-lived PASS receipt.
4. The guarded MCP verifies and atomically consumes that receipt while completing the bound objective.

Until all four conditions occur, the honest product status is **BLOCKED**.

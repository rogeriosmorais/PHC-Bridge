# Product Protocols

`causal-standing.v1.json` is the active standing protocol. `scripted-locomotion.v3.json` is the current Stage 2 scripted-locomotion protocol; v1 and v2 remain immutable historical authorities for evidence captured under those versions.

The `standing-v0.v1.json`, `standing-v0.v2.json`, and `impact-policy.v1.json`
files are retained byte-for-byte as historical records of the retired signed-
receipt workflow. They are not active product authority and their unit tests do
not constitute product progress.

Protocol rules:

1. Commit a protocol version before changing runtime behavior for that version.
2. Never edit a locked protocol. Add a new version and explain the change.
3. Runtime code emits raw observations, not a product verdict.
4. A product verdict requires the protocol's positive and negative variants.
5. Failed runs are retained; selecting only the best run is forbidden.
6. Everything required by an active protocol must exist inside this checkout.

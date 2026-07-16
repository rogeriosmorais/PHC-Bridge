# Stage 2 Hardening Completion Evidence

Date: 2026-07-16
Source commit: `68f86ffe8807424313bc2004021c9c28066cdce7`
Protocol: `product-gates/scripted-locomotion.v3.json`
Model SHA-256: `c8df64f75e10b3f71766895c054466db1918582ba5a5a85538a1dec8594bbe19`

## Policy tensor parity

A clean UE 5.7 `Normal` fixture captured the first policy-input provenance and runtime buffers. The independent Python oracle rebuilt all three policy inputs at tolerance `1e-5`:

- `self_observation`: 358 values, zero mismatches, max absolute error below `3.0e-8`;
- `mimic_target_poses`: 6,495 values, zero mismatches, max absolute error below `3.0e-8`;
- `terrain`: 256 values, zero mismatches, max absolute error below `5.0e-8`.

The captured provenance and runtime snapshot are retained beside the parity report so the result can be reproduced without Unreal.

## Causal locomotion and determinism

Three clean `Normal` fixtures ran in separate UE 5.7 processes. All three fixture/orchestration verdicts were `PASS`. Authority was identical across repetitions:

- source commit, model hash, normalized protocol hash, and environment authority digest matched;
- `policy-input-snapshot.json` was byte-identical in all three runs;
- every preregistered causal endpoint had zero numerical spread.

The determinism campaign verdict is nevertheless `FAIL`, not `PASS`, because every run consistently violated the v3 behavioral bounds:

- root-to-shell progress ratio: `3.4357209339978003` (maximum `1.4`);
- lateral route displacement: `330.0643880172959 cm` (maximum `60 cm`);
- average tracking error: `348.9770847121456 cm` (maximum `60 cm`);
- final tracking error: `599.7084281748643 cm` (maximum `75 cm`);
- maximum tracking error: `646.4908173630495 cm` (maximum `120 cm`).

This is a reproducible physical-root tracking failure, not run-to-run nondeterminism. The protocol was not weakened after observing the result.

## Files

- `policy-input-provenance.68f86ffe.json`
- `policy-input-snapshot.68f86ffe.json`
- `policy-tensor-parity.68f86ffe.json`
- `environment-fingerprint.68f86ffe.json`
- `fixture-validation.normal-1.68f86ffe.json`
- `scripted-locomotion-v3-causal-runs.68f86ffe.json`
- `scripted-locomotion-v3-determinism.68f86ffe.json`

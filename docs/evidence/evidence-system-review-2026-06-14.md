# Evidence System Review - 2026-06-14

## Scope

This review covers the latest evidence-based progress system implementation in PHC-Bridge:

- C++ evidence classifier, summary sidecar, collector, and terminal proof emitter.
- Python repository evidence collector and segment row contract tests.
- Runtime proof artifact/log reading path used after smoke and proof runs.
- Graph progress metadata that is currently used to judge whether work is really moving forward.

The review intentionally did not use the graph review workflow. It used local source inspection, focused tests, and graph metadata checks.

## Readiness Verdict

The system is directionally good, but it is not ready to be treated as a no-doubt progress meter.

The C++ classifier and summary/emitter mechanics are promising: they preserve a strict verdict vocabulary, require all five architecture segments for product-success candidate, and keep terminal proof JSON separate from the new evidence summary. However, the end-to-end evidence path is not robust yet because the Python collector is currently broken, the C++ collector can combine unrelated latest artifacts, and some segment activity rules are weaker than the baseline plan promises.

The current state should be treated as "evidence infrastructure under hardening," not as a reliable gate for continuing feature development without doubt.

## Verification Results

Commands run from `F:\NewEngine-AgentB`:

| Command | Result | Notes |
| --- | --- | --- |
| `pytest scripts\tests\test_collect_evidence.py scripts\tests\test_segment_evidence_row_contract.py` | Failed | 14 failed, 3 passed. Failures start at `NameError: name 're' is not defined` in `scripts\collect_evidence.py`. |
| `python scripts\collect_evidence.py` | Failed | Import-time failure at `LOG_VERDICT_RE = re.compile(...)`. |
| `python scripts\read_logs.py` | Passed | Reads latest UE log and copies PhysAnim lines, but now emits timestamped logger output. |
| `python -m py_compile scripts\collect_evidence.py scripts\read_logs.py scripts\physanim_logger.py scripts\gpt_web_ui.py Training\physanim\export_onnx.py` | Passed | Does not catch runtime import name failures caused by postponed annotations and module-level `re` usage. |
| `.\scripts\build.ps1 -Test "PhysAnim.EvidenceClassifier"` | Passed | 3 automation tests found and passed. |
| `.\scripts\build.ps1 -Test "PhysAnim.EvidenceCollector"` | Passed | 1 automation test found and passed, but the current contract is too permissive. |
| `.\scripts\build.ps1 -Test "PhysAnim.EvidenceSummary"` | Passed | 16 automation tests found and passed. |
| `mcp graph analyze done_integrity` | Passed | No done-integrity issues. |
| `mcp graph analyze status_flow` | Passed | 100 percent status-flow compliance. |
| `mcp graph analyze code_sync` | Not clean | Reports many stale source refs, many done nodes without `testFiles`, and stale code index. |
| `mcp graph analyze harness_scan` | Failed | `ENOENT: no such file or directory, scandir 'F:\NewEngine-AgentB\src'`. |

## Findings

### Blocker: Python evidence collector is broken

`scripts\collect_evidence.py` now imports only `physanim_logger`, but still uses `argparse`, `json`, `re`, `Path`, `Any`, `Dict`, `Iterable`, `List`, `Optional`, `Sequence`, and `Tuple`.

Key locations:

- `scripts\collect_evidence.py:3`
- `scripts\collect_evidence.py:39`
- `scripts\collect_evidence.py:52`
- `scripts\collect_evidence.py:532`

Impact:

- The one command intended to produce the current evidence report cannot run.
- The focused collector tests fail before they can validate contradiction and missing-evidence behavior.
- Evidence cannot be trusted as an operational gate while its CLI is down.

Recommended fix:

- Restore the standard-library imports.
- Keep logger integration only if it preserves stdout behavior expected by tests and callers.
- Ensure the default-script-location test remains portable when the script is copied without `physanim_logger.py`.

### High: C++ collector can mix attempts

`PhysAnimEvidenceCollector::Collect` loads the latest terminal artifact, latest evidence summary, and latest log independently:

- `PhysAnimEvidenceCollector.cpp:333`
- `PhysAnimEvidenceCollector.cpp:358`
- `PhysAnimEvidenceCollector.cpp:383`
- `PhysAnimEvidenceCollector.cpp:466`

The current test accepts `attempt-new` terminal evidence with a separate `summary-new` summary:

- `PhysAnimEvidenceCollector.Tests.cpp:214`
- `PhysAnimEvidenceCollector.Tests.cpp:228`

Impact:

- A report can merge stale or unrelated terminal, summary, and log evidence.
- This can create false confidence or false blockers.
- It directly undermines the purpose of attempt UUIDs.

Recommended fix:

- Add an optional requested attempt UUID to collector input.
- When no attempt is requested, choose one authoritative latest attempt and require all loaded evidence to match it.
- Treat mismatched evidence as insufficient or contradictory, not as progress.
- Add a regression test where latest files disagree and product progress is refused.

### High: PHC Policy active evidence is not harsh enough

The baseline plan says PHC Policy `Active` requires successful inference with finite, non-empty action output. Current emitter logic classifies policy activity using inference attempts and success count only:

- `PhysAnimProofArtifactEmitter.cpp:102`
- `PhysAnimProofArtifactEmitter.cpp:109`

It serializes stricter fields but does not require them for activity:

- `PhysAnimProofArtifactEmitter.cpp:392`
- `PhysAnimProofArtifactEmitter.cpp:396`
- `PhysAnimProofArtifactEmitter.cpp:399`
- `PhysAnimProofArtifactEmitter.cpp:400`

Impact:

- A policy segment can be marked active even when model-loaded, finite-buffer, or action-output evidence is weak.
- Product-success candidate can be closer than the evidence really supports.

Recommended fix:

- Require model loaded, finite buffers, at least one successful inference, at least one action sample, and finite/non-empty action magnitude evidence.
- Add negative tests for success count with missing model, non-finite buffers, and zero action samples.

### Medium: Segment row schema contract drift

The standalone row spec/test expects `required_metrics` and `provenance`:

- `plans\stage1\10-specs\segment_evidence_row.md:16`
- `plans\stage1\10-specs\segment_evidence_row.md:19`
- `scripts\tests\test_segment_evidence_row_contract.py:7`

The real C++ summary emits `metrics` and `source_provenance`:

- `PhysAnimEvidenceSummary.cpp:356`
- `PhysAnimEvidenceSummary.cpp:365`

Impact:

- The Python row contract does not validate the real emitted artifact shape.
- A passing row test can coexist with an incompatible production artifact schema.

Recommended fix:

- Decide one schema vocabulary.
- Prefer validating the real emitted summary shape used by C++ unless the C++ artifact is intentionally renamed.
- Add a fixture-based Python contract test using a representative evidence summary segment.

### Medium: Graph progress metadata is not enough by itself

Graph flow checks pass:

- `done_integrity`: passed.
- `status_flow`: 100 percent compliance.

But `code_sync` reports many stale source refs and done nodes without `testFiles`, including evidence-baseline nodes. The code index is stale relative to current `HEAD`.

Impact:

- The graph can say work is done while traceability to current code/tests is incomplete.
- It is useful flow evidence, but not a no-doubt progress signal.

Recommended fix:

- Attach test files to evidence-hardening tasks as they finish.
- Reindex or repair source refs after the code and docs settle.
- Do not present graph completion counts as proof of real runtime progress without artifact/test corroboration.

## Positive Evidence

- C++ classifier tests passed and the classifier order is conservative: contradiction beats success, missing durable terminal proof yields insufficient evidence, dirty truth flags block success, and all architecture segments are required for product-success candidate.
- C++ evidence summary tests passed and cover sidecar writing, serialization, backward-compatible terminal artifact shape, and segment state serialization for several segments.
- Raw `UE_LOG(` no longer appears in current plugin source.
- The existing Python collector test suite is strong in intent: it covers attempt UUID filtering, contradiction detection, missing summary behavior, weak terminal-proof pass logs, and critical stability metric validation.

## Required Hardening Sequence

1. Restore the Python collector CLI and tests.
2. Bind C++ evidence collection to a single attempt UUID.
3. Tighten PHC Policy activity classification.
4. Align the Python segment row schema contract with real emitted evidence summary artifacts.
5. Record graph traceability limitations and add test file references to evidence-hardening graph tasks.

Until those are complete, use evidence reports as diagnostic guidance only, not as final proof that product development is making validated progress.

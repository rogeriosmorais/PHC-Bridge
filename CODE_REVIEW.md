# Code Review — NewEngine Codebase

Full line-by-line review of all source files across Training (Python) and PhysAnimUE5 Plugin (C++).

---

## 🔴 Bugs

### 1. Duplicate bones in SMPL observation list

[PhysAnimBridge.cpp:72-103](file:///F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBridge.cpp#L72-L103) — `MakeSmplObservationBones()` lists **`hand_l` and `hand_r` twice each** (24 entries total). This is intentional for the SMPL 24-body layout where joints 22/23 ("L_Hand"/"R_Hand") map to the collapsed distal hand, but the mapping is misleading: both index 20 and 22 read from `hand_l`, and both 21 and 23 read from `hand_r`. In `GatherCurrentBodySamples`, this means the observation vector slots for joints 22–23 are **exact copies** of joints 20–21. This only works if the training side makes the same assumption. If the ProtoMotions observation builder uses SMPL `L_Hand`/`R_Hand` as distinct finger-tip proxies, the observation will be silently wrong.

> [!WARNING]
> This is the most likely source of a subtle training↔runtime mismatch. Verify against the ProtoMotions observation builder that indices 20–23 of the body state truly duplicate the wrist/hand bodies.

### 2. `test_retarget.py` uses wrong joint index for `L_Elbow`

[test_retarget.py:122](file:///F:/NewEngine/Training/tests/test_retarget.py#L122) — The test comment says `L_Elbow is joint index 18`, and the code sets `smpl_pose[18 * 3 : 18 * 3 + 3]`. But according to the SMPL joint convention, L_Elbow is **joint index 18** *only in the 0-indexed standard*. The SKILL.md skill defines it at index 18. Verify this matches the retarget module's naming — if the retarget module uses 1-indexed joint IDs, the test will flex the wrong joint.

### 3. `const_cast` in `GatherCurrentBodySamples`

[PhysAnimComponent.cpp:1063](file:///F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp#L1063) — `const_cast<USkeletalMeshComponent*>(SkeletalMesh)` to call `GetPhysicsLinearVelocity`, which is non-const in UE5. This is functionally safe today but fragile — a future UE5 update could change the semantics of these functions.

---

## 🟡 Risks & Correctness Concerns

### 4. Self-observation dimension math is fragile

`BuildSelfObservation` packs: 1 (root height) + 23×3 (local positions) + 24×6 (tan-norm rotations) + 24×3 (linear velocities) + 24×3 (angular velocities) = **1 + 69 + 144 + 72 + 72 = 358**. This matches `SelfObsSize = 358`. Good — but there is no compile-time or unit-level assertion verifying the math. The post-hoc size check at l.362 catches it at runtime, but a `static_assert` or dimensionality constant expression would be safer.

### 5. `MimicTargetPoses` per-step packing logic is complex and untested for edge cases

`BuildMimicTargetPoses` packs per future step: 24×3 (relative pos) + 24×3 (root-relative pos) + 24×6 (relative tan-norm) + 24×6 (heading-relative tan-norm) + 1 (time) = 72 + 72 + 144 + 144 + 1 = 433. Total: 433 × 15 = 6495. This matches, but there is no unit test exercising the *content* of the packed values (only the size is checked in `FPhysAnimObservationPackingTest`).

### 6. SMPL↔UE5 coordinate conversion is "axis-remap" style, not a simple basis swap

`SmplVectorToUe` maps `(X, Y, Z)_SMPL → (Z, X, Y)_UE`. This is `Y-up → Z-up` via a cyclic permutation. The inverse `UeVectorToSmpl` correctly inverts it. **However**, the quaternion roundtrip `UeQuaternionToSmpl(SmplQuaternionToUe(Q))` would lose information if the `MakeFromXZ` reconstruction is ambiguous — the roundtrip test only checks 1e-3 tolerance. For production use, verify this tolerance is acceptable for the policy's expected sensitivity.

### 7. No `torch.no_grad()` in `Stage1PolicyWrapper.forward()`

[export_onnx.py:114-137](file:///F:/NewEngine/Training/physanim/export_onnx.py#L114-L137) — The wrapper's `forward()` does not disable gradient tracking. This isn't a bug (inference during export is wrapped in `torch.onnx.export`), but calling `wrapper(...)` outside of export (as in `test_load_stage1_policy_returns_real_pretrained_actor`) unnecessarily builds a gradient graph. Low risk, minor perf concern in tests.

### 8. `RecreatePhysicsState()` on every bridge activation

[PhysAnimComponent.cpp:1392](file:///F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp#L1392) — `SkeletalMesh->RecreatePhysicsState()` is expensive and triggers a full physics-body teardown and rebuild. If the bridge rapidly deactivates/reactivates (e.g., during safe-mode transitions), this could cause visible hitches or transient physics instability.

---

## 🟠 Architectural Gaps

### 9. `physanim.retarget` module does not exist

[test_retarget.py](file:///F:/NewEngine/Training/tests/test_retarget.py) imports `physanim.retarget`, which is marked as "TDD — not yet implemented". The entire test file is `skipif(not HAS_RETARGET)`. This is a known gap from the TDD workflow, but it means there is **zero retargeting production code** and no Python-side coordinate conversion between SMPL and UE5. The C++ bridge implements its own retargeting, but there's no cross-validation between the two sides.

### 10. ProtoMotions compatibility shims lack tests with real ProtoMotions

`test_base_simulator_compat.py` and `test_isaaclab_compat.py` test against fake/mock imports. These verify the shim logic, but there's no integration test confirming the shims work with the actual ProtoMotions codebase. If ProtoMotions updates its internal structure, these shims could silently break.

### 11. No `pytest` configuration file or `conftest.py`

The Training tests rely on manual `sys.path` manipulation. There's no `pyproject.toml`, `setup.py`, or `conftest.py` in the `Training/` directory. This makes running tests outside of the specific directory structure fragile (e.g., CI systems).

### 12. Compiled plugin artifacts checked in (or missing `.gitignore` coverage)

`_build/PhysAnimPlugin/Binaries/` and `_build/PhysAnimPlugin/Intermediate/` are not explicitly `.gitignore`d as a pattern. The top-level `_build/` is ignored, which covers it, but compiled `.dll`, `.pdb`, and `.patch_*.exe` files appear in the `PhysAnimUE5/Plugins/PhysAnimPlugin/Binaries/` and `Intermediate/` directories. The `.gitignore` covers `PhysAnimUE5/Plugins/PhysAnimPlugin/Binaries/` and `Intermediate/` so these *should* be excluded — **verify with `git status`** that they aren't accidentally tracked.

### 13. Missing `data/` directory structure and `.gitkeep`

`AGENTS.md` references `Training/data/` for AMASS/Mixamo data, but no `Training/data/` directory exists (only the `.gitignore` entry for `Training/data/amass/`). Minor — the directory would be created when data is first downloaded.

---

## 🔵 Style & Maintenance

### 14. Mixed line-endings

Several files use Windows-style `\r\n` while others use Unix `\n` (e.g., `test_retarget.py` vs `PhysAnimBridge.cpp`). The `.uproject` file mixes both within the same file. Consider adding a `.editorconfig` or `.gitattributes` to normalize line endings.

### 15. `StabilizationSettings` equality operator is large and manual

[PhysAnimComponent.h:77-96](file:///F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h#L77-L96) — The `operator==` on `FPhysAnimStabilizationSettings` has 17 comparisons. If a new field is added and the operator isn't updated, stale-settings detection will silently break. Consider a `FMemory::Memcmp` guard or a version counter.

### 16. `test_onnx_export.py` hardcodes opset expectation

[test_onnx_export.py:95](file:///F:/NewEngine/Training/tests/test_onnx_export.py#L95) — `assert accepted_opset == 17` will break if the export is later changed to a different opset. The test should check that the opset is one of the configured fallbacks.

### 17. Per-frame TMap allocation in `ConvertModelActionsToControlRotations`

[PhysAnimBridge.cpp:577](file:///F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBridge.cpp#L577) — `OutControlRotations.Reset()` + `Reserve` + 21 `Add` calls. This allocates every frame. For a hot path, consider reusing the map or using a static array with a name-index mapping.

---

## Summary

| Severity | Count | Key Items |
|---|---|---|
| 🔴 Bug | 3 | Duplicate SMPL observation bones, test joint index, const_cast |
| 🟡 Risk | 5 | Fragile dimension math, untested packing content, quaternion tolerance, gradient graph, physics state recreation |
| 🟠 Gap | 5 | Missing retarget module, mock-only compat tests, no pytest config, build artifacts, missing data dir |
| 🔵 Style | 4 | Line endings, large equality op, hardcoded opset, per-frame allocation |

---

## Assessment By Validity, Stage, And Severity

### Valid

#### Stage 1 — Phase 0 / Training-Side Contract

##### High

9. `physanim.retarget` module does not exist
   Verdict: valid.
   Reason: `Training/tests/test_retarget.py` is entirely skipped behind `HAS_RETARGET`, and there is no `Training/physanim/retarget.py`.
   Action: implement `physanim.retarget` under TDD and cross-check its mapping and frame conversion against the C++ bridge.

##### Medium

10. ProtoMotions compatibility shims lack tests with real ProtoMotions
    Verdict: valid.
    Reason: the current tests exercise import fallback logic with fakes only; there is no integration coverage against the pinned local ProtoMotions checkout.
    Action: add an optional integration test path that imports the real ProtoMotions tree and exercises the shim entry points against the checked-in revision.

11. No `pytest` configuration file or `conftest.py`
    Verdict: valid.
    Reason: there is no `pyproject.toml`, `pytest.ini`, or `conftest.py`, and the tests currently patch `sys.path` manually.
    Action: add a minimal pytest configuration layer so imports, markers, and shared fixtures are defined centrally instead of per-test.

16. `test_onnx_export.py` hardcodes opset expectation
    Verdict: valid.
    Reason: the export path already supports fallback opsets `(17, 16, 15)`, but the test hardcodes `17`.
    Action: change the assertion to accept `loaded.spec.opset_fallbacks` or to validate the chosen configured opset instead of pinning a single literal.

##### Low

7. No `torch.no_grad()` in `Stage1PolicyWrapper.forward()`
   Verdict: valid.
   Reason: the wrapper is used for inference/export flows, and the direct test call to `loaded.wrapper(...)` currently builds a grad graph unnecessarily.
   Action: wrap non-training call sites in `torch.no_grad()` or `torch.inference_mode()`. If the wrapper is guaranteed to stay inference-only, making that explicit inside the wrapper is also acceptable.

14. Mixed line-endings
    Verdict: valid.
    Reason: `git ls-files --eol` shows `Training/tests/test_retarget.py` as `w/crlf` and `PhysAnimUE5/PhysAnimUE5.uproject` as `w/mixed`.
    Action: add `.gitattributes` and optionally `.editorconfig`, then normalize the affected files once to the chosen policy.

#### Stage 1 — Phase 1 / UE Bridge Runtime

##### Medium

4. Self-observation dimension math is fragile
   Verdict: valid.
   Reason: the runtime size check exists, but the `358` contract is still hand-maintained.
   Action: derive `SelfObsSize` from named per-field constants and add a `static_assert` so packing changes fail at compile time.

5. `MimicTargetPoses` per-step packing logic is complex and untested for edge cases
   Verdict: valid.
   Reason: the current automation test checks only tensor size, not the actual packed contents.
   Action: add deterministic packing tests that assert representative values for relative positions, root-relative positions, relative rotations, heading-aligned rotations, and the appended time scalar.

15. `StabilizationSettings` equality operator is large and manual
    Verdict: valid.
    Reason: the comparison is hand-maintained across many fields, so adding a new field without updating `operator==` would silently break stale-settings detection.
    Action: replace the inline comparison with a dedicated helper and add a regression test that covers every field. Do not switch this to raw `FMemory::Memcmp`, because the struct contains floats compared with tolerance.

##### Low

3. `const_cast` in `GatherCurrentBodySamples`
   Verdict: valid, but low severity.
   Reason: this is not a current correctness bug, but the `const_cast` is avoidable and makes the helper more brittle than it needs to be.
   Action: remove the `const` qualifier from `GatherCurrentBodySamples` and use a non-const `USkeletalMeshComponent*` directly when reading physics velocities.

8. `RecreatePhysicsState()` on every bridge activation
   Verdict: valid, low severity.
   Reason: the call is present on every transition into `BridgeActive`, so repeated safe-mode toggles would rebuild physics state each time.
   Action: gate `RecreatePhysicsState()` behind an actual state-change requirement, or restructure activation so repeated `ReadyForActivation <-> BridgeActive` transitions do not force a full rebuild unless collision/physics mode genuinely changed.

17. Per-frame TMap allocation in `ConvertModelActionsToControlRotations`
    Verdict: valid, low severity.
    Reason: `ConvertModelActionsToControlRotations` is called from the tick path and currently fills a fresh local `TMap<FName, FQuat>` each time.
    Action: if this shows up in profiling, replace the transient map with a reused member container or a fixed-order array/struct keyed by the frozen controlled-bone order.

#### Stage 2

- No valid Stage 2-specific issues were identified in this review.

### Invalid

#### Stage 1

1. Duplicate bones in SMPL observation list
   Verdict: not valid as a bug.
   Reason: the duplicate `hand_l`/`hand_r` reads are the explicitly frozen Stage 1 surrogate rule in the bridge and retargeting specs: Manny `hand_l` stands in for both `L_Wrist` and `L_Hand`, and `hand_r` stands in for both `R_Wrist` and `R_Hand`.
   Action: no runtime fix required. The only useful cleanup is to add a code comment and regression test that points back to the frozen surrogate rule so the duplication is not mistaken for an accident.

2. `test_retarget.py` uses wrong joint index for `L_Elbow`
   Verdict: not valid.
   Reason: the project SMPL table defines `L_Elbow` at index `18`, and the test writes index `18`. The 1-indexed concern does not apply anywhere in the current code because `physanim.retarget` does not exist yet.
   Action: no fix required.

6. SMPL↔UE5 coordinate conversion is "axis-remap" style, not a simple basis swap
   Verdict: not valid as written.
   Reason: the vector mapping is the frozen basis mapping from the Stage 1 specs, and quaternion conversion already goes through a basis-reconstruction path rather than a raw component shuffle. The review point is really about test depth, not a demonstrated defect in the current implementation.
   Action: no code fix required. If more confidence is wanted, broaden the roundtrip tests to cover more samples and tighter assertions.

12. Compiled plugin artifacts checked in (or missing `.gitignore` coverage)
    Verdict: not valid.
    Reason: `.gitignore` already covers `PhysAnimUE5/Plugins/PhysAnimPlugin/Binaries/` and `Intermediate/`, and `git ls-files` shows no tracked files under those paths.
    Action: no fix required.

13. Missing `data/` directory structure and `.gitkeep`
    Verdict: not valid.
    Reason: this repository is still a planning scaffold, and nothing currently depends on an empty `Training/data/` directory existing ahead of dataset download.
    Action: no fix required.

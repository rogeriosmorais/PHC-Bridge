from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path
from typing import Iterable

try:
    from scripts.evaluate_manny_local_frame_roundtrip import (
        ANGULAR_TOLERANCE_DEGREES,
        COMPONENT_AXIS_MODE,
        EvaluationError,
        WORLD_AXIS_MODE,
        evaluate_trace,
    )
except ModuleNotFoundError:  # Direct execution via `python scripts/...`.
    from evaluate_manny_local_frame_roundtrip import (
        ANGULAR_TOLERANCE_DEGREES,
        COMPONENT_AXIS_MODE,
        EvaluationError,
        WORLD_AXIS_MODE,
        evaluate_trace,
    )

EVALUATION_SCHEMA = "physanim-action-adapter-factorial-evaluation/v1"
EXPECTED_MODES = {
    "A_world_captured": WORLD_AXIS_MODE,
    "B_component_captured": COMPONENT_AXIS_MODE,
    "C_world_bind": WORLD_AXIS_MODE,
    "D_component_bind": COMPONENT_AXIS_MODE,
}


class ActionAdapterFactorialError(ValueError):
    pass


def _read_json(path: Path, label: str) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ActionAdapterFactorialError(f"Cannot read {label}: {exc}") from exc
    if not isinstance(value, dict):
        raise ActionAdapterFactorialError(f"{label} must be a JSON object")
    return value


def _read_first_jsonl(path: Path, label: str) -> dict:
    try:
        line = next(line for line in path.read_text(encoding="utf-8").splitlines() if line.strip())
        value = json.loads(line)
    except (OSError, StopIteration, json.JSONDecodeError) as exc:
        raise ActionAdapterFactorialError(f"Cannot read first {label} row: {exc}") from exc
    if not isinstance(value, dict):
        raise ActionAdapterFactorialError(f"First {label} row must be an object")
    return value


def _read_jsonl(path: Path, label: str) -> list[dict]:
    rows: list[dict] = []
    try:
        for line in path.read_text(encoding="utf-8").splitlines():
            if line.strip():
                value = json.loads(line)
                if not isinstance(value, dict):
                    raise ActionAdapterFactorialError(f"{label} contains a non-object row")
                rows.append(value)
    except (OSError, json.JSONDecodeError) as exc:
        raise ActionAdapterFactorialError(f"Cannot read {label}: {exc}") from exc
    if not rows:
        raise ActionAdapterFactorialError(f"{label} is empty")
    return rows


def _finite(value: object, label: str) -> float:
    try:
        number = float(value)
    except (TypeError, ValueError) as exc:
        raise ActionAdapterFactorialError(f"{label} is not numeric") from exc
    if not math.isfinite(number):
        raise ActionAdapterFactorialError(f"{label} is not finite")
    return number


def _same_bytes(paths: Iterable[Path]) -> bool:
    payloads = [path.read_bytes() for path in paths]
    return all(payload == payloads[0] for payload in payloads[1:])


def _arm_metrics(trace: dict, single: dict) -> dict:
    decisive = [entry for entry in trace.get("controls", []) if entry.get("decisive_one_to_one") is True]
    if len(decisive) != 19:
        raise ActionAdapterFactorialError("Each arm must contain 19 decisive controls")
    identity: list[float] = []
    actual: list[float] = []
    probes: list[float] = []
    actual_excess: list[float] = []
    probe_excess: list[float] = []
    for entry in decisive:
        cases = entry.get("roundtrip_cases")
        if not isinstance(cases, list) or len(cases) != 8:
            raise ActionAdapterFactorialError("Each decisive control must contain eight cases")
        errors = [_finite(case.get("angular_error_degrees"), "roundtrip error") for case in cases]
        identity.append(errors[0])
        actual.append(errors[1])
        probes.extend(errors[2:])
        actual_excess.append(abs(errors[1] - errors[0]))
        probe_excess.extend(abs(error - errors[0]) for error in errors[2:])
    metrics = single.get("metrics", {})
    return {
        "maximum_identity_error_degrees": max(identity, default=0.0),
        "maximum_actual_error_degrees": max(actual, default=0.0),
        "maximum_probe_error_degrees": max(probes, default=0.0),
        "maximum_actual_error_minus_identity_residual_degrees": max(actual_excess, default=0.0),
        "maximum_probe_error_minus_identity_residual_degrees": max(probe_excess, default=0.0),
        "maximum_policy_neutral_vs_bind_degrees": _finite(
            metrics.get("maximum_policy_neutral_vs_action_bind_parent_relative_angular_delta_degrees"),
            "neutral-versus-bind metric",
        ),
    }


def evaluate_factorial(
    arm_a: Path | str,
    arm_b: Path | str,
    arm_c: Path | str,
    arm_d: Path | str,
) -> dict:
    roots = {
        "A_world_captured": Path(arm_a).resolve(),
        "B_component_captured": Path(arm_b).resolve(),
        "C_world_bind": Path(arm_c).resolve(),
        "D_component_bind": Path(arm_d).resolve(),
    }
    manifests: dict[str, dict] = {}
    traces: dict[str, dict] = {}
    singles: dict[str, dict] = {}
    first_policy_snapshots: dict[str, dict] = {}
    readback_ratios: dict[str, float] = {}

    for name, root in roots.items():
        manifests[name] = _read_json(root / "manifest.json", f"{name} manifest")
        trace_path = root / "manny-local-frame-roundtrip.json"
        traces[name] = _read_json(trace_path, f"{name} trace")
        try:
            singles[name] = evaluate_trace(trace_path)
        except EvaluationError as exc:
            raise ActionAdapterFactorialError(f"{name} trace is invalid: {exc}") from exc
        if singles[name].get("status") != "VALID":
            raise ActionAdapterFactorialError(f"{name} trace did not validate")
        if traces[name].get("configured_action_axis_mode") != EXPECTED_MODES[name]:
            raise ActionAdapterFactorialError(f"{name} configured action-axis mode is wrong")
        if traces[name].get("effective_action_axis_mode") != EXPECTED_MODES[name]:
            raise ActionAdapterFactorialError(f"{name} effective action-axis mode is wrong")
        first_policy_snapshots[name] = _read_json(
            root / "policy-input-snapshot.json", f"{name} first-policy snapshot"
        )
        policy_rows = _read_jsonl(root / "policy.jsonl", f"{name} policy")
        attempts = sum(int(row.get("target_write_attempt_count", 0)) for row in policy_rows)
        matches = sum(int(row.get("target_readback_match_count", 0)) for row in policy_rows)
        if attempts <= 0:
            raise ActionAdapterFactorialError(f"{name} has no target writes")
        readback_ratios[name] = matches / attempts
        if abs(readback_ratios[name] - 1.0) > 1.0e-12:
            raise ActionAdapterFactorialError(f"{name} target readback ratio is not 1.0")

    manifest_lock_fields = (
        "source_commit",
        "source_tree_dirty",
        "model_onnx_sha256",
        "protocol_path",
        "variant",
        "reference_pelvis_height_cm",
        "capture_window_sec",
    )
    reference_manifest = manifests["A_world_captured"]
    for name, manifest in manifests.items():
        for field in manifest_lock_fields:
            if manifest.get(field) != reference_manifest.get(field):
                raise ActionAdapterFactorialError(f"Locked manifest field {field} differs in {name}")

    snapshot_paths = [root / "policy-input-snapshot.json" for root in roots.values()]
    if not _same_bytes(snapshot_paths):
        raise ActionAdapterFactorialError(
            "First-policy input/output snapshots differ across arms"
        )
    reference_snapshot = first_policy_snapshots["A_world_captured"]
    for name, snapshot in first_policy_snapshots.items():
        for field in ("self_observation", "mimic_target_poses", "terrain"):
            if snapshot.get(field) != reference_snapshot.get(field):
                raise ActionAdapterFactorialError(
                    f"First-policy input field {field} differs in {name}"
                )
        if snapshot.get("actions") != reference_snapshot.get("actions"):
            raise ActionAdapterFactorialError(
                f"First-policy model output actions differ in {name}"
            )

    metrics = {
        name: _arm_metrics(traces[name], singles[name])
        for name in roots
    }
    tolerance = ANGULAR_TOLERANCE_DEGREES
    a = metrics["A_world_captured"]
    b = metrics["B_component_captured"]
    c = metrics["C_world_bind"]
    d = metrics["D_component_bind"]
    checks = {
        "A_has_identity_residual": a["maximum_identity_error_degrees"] > tolerance,
        "A_has_world_axis_excess": max(
            a["maximum_actual_error_minus_identity_residual_degrees"],
            a["maximum_probe_error_minus_identity_residual_degrees"],
        ) > tolerance,
        "B_preserves_captured_neutral_identity_residual": abs(
            b["maximum_identity_error_degrees"] - a["maximum_identity_error_degrees"]
        ) <= tolerance,
        "B_removes_nonidentity_excess": max(
            b["maximum_actual_error_minus_identity_residual_degrees"],
            b["maximum_probe_error_minus_identity_residual_degrees"],
        ) <= tolerance,
        "C_removes_identity_residual": c["maximum_identity_error_degrees"] <= tolerance,
        "C_retains_world_axis_excess": max(
            c["maximum_actual_error_degrees"], c["maximum_probe_error_degrees"]
        ) > tolerance,
        "D_is_exact_inverse": max(
            d["maximum_identity_error_degrees"],
            d["maximum_actual_error_degrees"],
            d["maximum_probe_error_degrees"],
        ) <= tolerance,
        "captured_neutral_arms_are_nonbind": min(
            a["maximum_policy_neutral_vs_bind_degrees"],
            b["maximum_policy_neutral_vs_bind_degrees"],
        ) > tolerance,
        "bind_neutral_arms_use_bind": max(
            c["maximum_policy_neutral_vs_bind_degrees"],
            d["maximum_policy_neutral_vs_bind_degrees"],
        ) <= tolerance,
    }
    supported = all(checks.values())
    return {
        "schema_version": EVALUATION_SCHEMA,
        "status": "VALID",
        "hypothesis_verdict": "SUPPORTED" if supported else "FALSIFIED",
        "authority": "DEVELOPMENT_EVIDENCE_ONLY",
        "product_success": False,
        "tolerance_degrees": tolerance,
        "first_policy_input_exact": True,
        "first_policy_output_exact": True,
        "target_readback_ratios": readback_ratios,
        "checks": checks,
        "metrics": metrics,
        "arms": {name: str(root) for name, root in roots.items()},
    }


def _invalid_result(message: str) -> dict:
    return {
        "schema_version": EVALUATION_SCHEMA,
        "status": "INVALID",
        "hypothesis_verdict": "NOT_EVALUATED",
        "authority": "DEVELOPMENT_EVIDENCE_ONLY",
        "product_success": False,
        "error": message,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Evaluate the E18 2x2 action-adapter factorial")
    parser.add_argument("--arm-a", required=True)
    parser.add_argument("--arm-b", required=True)
    parser.add_argument("--arm-c", required=True)
    parser.add_argument("--arm-d", required=True)
    parser.add_argument("--output")
    args = parser.parse_args(argv)
    try:
        result = evaluate_factorial(args.arm_a, args.arm_b, args.arm_c, args.arm_d)
        exit_code = 0
    except ActionAdapterFactorialError as exc:
        result = _invalid_result(str(exc))
        exit_code = 2
    rendered = json.dumps(result, indent=2, sort_keys=True)
    if args.output:
        Path(args.output).write_text(rendered + "\n", encoding="utf-8")
    print(rendered)
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())

from __future__ import annotations

import argparse
import hashlib
import json
import math
import statistics
import sys
from pathlib import Path
from typing import Iterable


class EvaluationError(ValueError):
    pass


def _read_json(path: Path, label: str) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise EvaluationError(f"{label} does not exist: {path}") from exc
    except json.JSONDecodeError as exc:
        raise EvaluationError(f"{label} is not valid JSON: {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise EvaluationError(f"{label} must be a JSON object: {path}")
    return value


def _read_jsonl(path: Path, label: str, *, allow_empty: bool = False) -> list[dict]:
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except FileNotFoundError as exc:
        raise EvaluationError(f"{label} does not exist: {path}") from exc
    if not lines and not allow_empty:
        raise EvaluationError(f"{label} is empty: {path}")
    rows: list[dict] = []
    for line_number, line in enumerate(lines, start=1):
        try:
            row = json.loads(line)
        except json.JSONDecodeError as exc:
            raise EvaluationError(f"{label} line {line_number} is not valid JSON") from exc
        if not isinstance(row, dict):
            raise EvaluationError(f"{label} line {line_number} must be a JSON object")
        rows.append(row)
    return rows


def _require_fields(value: dict, fields: Iterable[str], label: str) -> None:
    missing = sorted(field for field in fields if field not in value)
    if missing:
        raise EvaluationError(f"{label} is missing required fields: {', '.join(missing)}")


def _validate_monotonic(rows: list[dict], label: str) -> None:
    previous_sequence = None
    previous_time = None
    for index, row in enumerate(rows):
        sequence = row.get("sequence")
        time_sec = row.get("time_sec")
        if isinstance(sequence, bool) or not isinstance(sequence, int):
            raise EvaluationError(f"{label} row {index} sequence must be an integer")
        if isinstance(time_sec, bool) or not isinstance(time_sec, (int, float)) or not math.isfinite(time_sec):
            raise EvaluationError(f"{label} row {index} time_sec must be finite")
        if previous_sequence is not None and sequence <= previous_sequence:
            raise EvaluationError(f"{label} sequence must be strictly increasing")
        if previous_time is not None and time_sec <= previous_time:
            raise EvaluationError(f"{label} time_sec must be strictly increasing")
        previous_sequence = sequence
        previous_time = float(time_sec)


def _integral(rows: list[dict], field: str) -> float:
    total = 0.0
    for previous, current in zip(rows, rows[1:]):
        dt = float(current["time_sec"]) - float(previous["time_sec"])
        total += dt * (float(previous[field]) + float(current[field])) * 0.5
    return total


def _all(rows: list[dict], predicate) -> bool:
    return bool(rows) and all(predicate(row) for row in rows)


def _protocol_digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def evaluate_manifest(manifest_path: Path | str) -> dict:
    manifest_path = Path(manifest_path).resolve()
    manifest = _read_json(manifest_path, "run manifest")
    _require_fields(
        manifest,
        (
            "schema_version",
            "fixture_authority",
            "run_id",
            "protocol_path",
            "variant",
            "repetition",
            "source_commit",
            "source_tree_dirty",
            "model_onnx_sha256",
            "reference_pelvis_height_cm",
            "standing_window_start_sec",
            "perturbation_time_sec",
            "physics_samples",
            "policy_samples",
            "render_capture",
            "render_nonblank_pixel_count",
        ),
        "run manifest",
    )
    if manifest["schema_version"] != "physanim-product-run/v1":
        raise EvaluationError("run manifest schema_version is unsupported")
    if manifest["fixture_authority"] not in {"EVALUATOR_UNIT_ONLY", "PRODUCT_RUN"}:
        raise EvaluationError("run manifest fixture_authority is unsupported")

    protocol_path = Path(manifest["protocol_path"]).resolve()
    protocol = _read_json(protocol_path, "product protocol")
    if protocol.get("schema_version") != "physanim-product-protocol/v1" or protocol.get("status") != "LOCKED":
        raise EvaluationError("product protocol is not a locked v1 protocol")
    if protocol.get("protocol_id") != "causal-standing" or protocol.get("version") != 1:
        raise EvaluationError("run manifest references the wrong product protocol")
    if manifest["variant"] not in protocol["variants"]:
        raise EvaluationError(f"unknown run variant: {manifest['variant']}")
    if not isinstance(manifest["repetition"], int) or manifest["repetition"] < 1:
        raise EvaluationError("run repetition must be a positive integer")
    if not isinstance(manifest["source_commit"], str) or len(manifest["source_commit"]) != 40:
        raise EvaluationError("source_commit must be a 40-character commit")
    if not isinstance(manifest["model_onnx_sha256"], str) or len(manifest["model_onnx_sha256"]) != 64:
        raise EvaluationError("model_onnx_sha256 must be a SHA-256 digest")

    run_dir = manifest_path.parent
    physics_path = (run_dir / manifest["physics_samples"]).resolve()
    policy_path = (run_dir / manifest["policy_samples"]).resolve()
    render_path = (run_dir / manifest["render_capture"]).resolve()
    for candidate, label in ((physics_path, "physics samples"), (policy_path, "policy samples"), (render_path, "render capture")):
        if run_dir not in candidate.parents:
            raise EvaluationError(f"{label} must stay inside the run directory")

    physics_rows = _read_jsonl(physics_path, "physics samples")
    policy_rows = _read_jsonl(policy_path, "policy samples", allow_empty=True)
    _validate_monotonic(physics_rows, "physics samples")
    _validate_monotonic(policy_rows, "policy samples")

    physics_required = protocol["sample_streams"]["physics"]["required_fields"]
    policy_required = protocol["sample_streams"]["policy"]["required_fields"]
    for index, row in enumerate(physics_rows):
        _require_fields(row, physics_required, f"physics row {index}")
    for index, row in enumerate(policy_rows):
        _require_fields(row, policy_required, f"policy row {index}")

    acceptance = protocol["acceptance"]
    start = float(manifest["standing_window_start_sec"])
    end = start + float(acceptance["standing_window_sec"])
    perturbation = float(manifest["perturbation_time_sec"])
    window_physics = [row for row in physics_rows if start <= float(row["time_sec"]) <= end]
    window_policy = [row for row in policy_rows if start <= float(row["time_sec"]) < end]
    pre_impulse = [row for row in window_physics if perturbation - acceptance["baseline_window_sec"] <= float(row["time_sec"]) < perturbation]
    post_impulse = [row for row in window_physics if perturbation <= float(row["time_sec"]) <= end]
    if not window_physics or float(window_physics[-1]["time_sec"]) - float(window_physics[0]["time_sec"]) < acceptance["standing_window_sec"]:
        raise EvaluationError("physics stream does not cover the standing window")
    if not pre_impulse or len(post_impulse) < 2:
        raise EvaluationError("physics stream does not cover baseline and post-perturbation windows")

    failed: list[str] = []

    def require(name: str, condition: bool) -> None:
        if not condition:
            failed.append(name)

    reference_height = float(manifest["reference_pelvis_height_cm"])
    require("reference_pelvis_height", reference_height > 0.0 and math.isfinite(reference_height))
    require("runtime_state", _all(window_physics, lambda row: row["runtime_state"] == acceptance["required_runtime_state"]))
    require("pelvis_height", _all(window_physics, lambda row: float(row["pelvis_height_cm"]) >= reference_height * acceptance["minimum_pelvis_height_ratio"]))
    require("root_tilt", _all(window_physics, lambda row: 0.0 <= float(row["root_tilt_deg"]) <= acceptance["maximum_root_tilt_deg"]))
    require("penetration", _all(window_physics, lambda row: 0.0 <= float(row["max_penetration_cm"]) <= acceptance["maximum_penetration_cm"]))
    require("support_gap", _all(window_physics, lambda row: 0.0 <= float(row["support_gap_ms"]) <= acceptance["maximum_support_gap_ms"]))
    require("critical_body_valid", _all(window_physics, lambda row: int(row["critical_body_valid_mask"]) == acceptance["required_critical_body_mask"]))
    require("critical_body_simulating", _all(window_physics, lambda row: int(row["critical_body_simulating_mask"]) == acceptance["required_critical_body_mask"]))
    require("support_body_valid", _all(window_physics, lambda row: int(row["support_body_valid_mask"]) == acceptance["required_support_body_mask"]))
    require("support_body_simulating", _all(window_physics, lambda row: int(row["support_body_simulating_mask"]) == acceptance["required_support_body_mask"]))
    require("root_simulating", _all(window_physics, lambda row: row["root_is_simulating"] is True))
    require("cmc_inactive", _all(window_physics, lambda row: row["cmc_active"] is False and row["cmc_tick_enabled"] is False and row["cmc_updated_component_is_null"] is True))
    require("capsule_collision", _all(window_physics, lambda row: int(row["capsule_collision_enabled"]) == 0))
    require("movement_reclaim", _all(window_physics, lambda row: int(row["movement_reclaim_count"]) == 0))
    require("shell_helper", _all(window_physics, lambda row: int(row["shell_helper_used_count"]) == 0))
    require("topology_change", _all(window_physics, lambda row: int(row["topology_change_count"]) == 0))

    expected_steps = acceptance["standing_window_sec"] * protocol["sample_streams"]["policy"]["expected_rate_hz"]
    require("policy_step_coverage", len(window_policy) >= math.floor(expected_steps * acceptance["minimum_policy_step_coverage"]))
    require("pose_search", _all(window_policy, lambda row: row["pose_search_valid"] is True and row["selected_animation"] == acceptance["required_pose_search_animation"]))
    require("inference", _all(window_policy, lambda row: row["inference_attempted"] is True and row["inference_succeeded"] is True))
    if manifest["variant"] == "Normal":
        require("conditioned_policy_action", any(float(row["conditioned_action_l2"]) > 0.0 for row in window_policy))

    attempted_writes = sum(int(row["target_write_attempt_count"]) for row in window_policy)
    matched_writes = sum(int(row["target_readback_match_count"]) for row in window_policy)
    match_ratio = matched_writes / attempted_writes if attempted_writes > 0 else 0.0
    max_readback_error = max(
        (float(row["target_readback_max_error_deg"]) for row in window_policy),
        default=math.inf,
    )
    require(
        "target_readback",
        attempted_writes > 0
        and match_ratio >= acceptance["minimum_target_readback_match_ratio"]
        and max_readback_error <= acceptance["maximum_target_readback_error_deg"],
    )

    baseline_rms = math.sqrt(sum(float(row["pose_rms_error_deg"]) ** 2 for row in pre_impulse) / len(pre_impulse))
    recovery_limit = max(acceptance["recovery_pose_rms_floor_deg"], baseline_rms * acceptance["recovery_pose_rms_baseline_multiplier"])
    recovery_deadline = perturbation + acceptance["recovery_deadline_sec"]
    recovered = False
    for candidate in post_impulse:
        candidate_time = float(candidate["time_sec"])
        if candidate_time > recovery_deadline:
            break
        hold_end = candidate_time + acceptance["recovery_hold_sec"]
        hold_rows = [row for row in post_impulse if candidate_time <= float(row["time_sec"]) <= hold_end]
        if hold_rows and float(hold_rows[-1]["time_sec"]) >= hold_end and _all(hold_rows, lambda row: float(row["pose_rms_error_deg"]) <= recovery_limit):
            recovered = True
            break
    require("recovery", recovered)

    require(
        "render_capture",
        render_path.is_file()
        and render_path.stat().st_size > 0
        and isinstance(manifest["render_nonblank_pixel_count"], int)
        and manifest["render_nonblank_pixel_count"] > 0,
    )

    return {
        "schema_version": "physanim-product-evaluation/v1",
        "run_id": manifest["run_id"],
        "variant": manifest["variant"],
        "repetition": manifest["repetition"],
        "status": "PASS" if not failed else "FAIL",
        "fixture_authority": manifest["fixture_authority"],
        "failed_criteria": failed,
        "recovery_auc": _integral(post_impulse, "pose_rms_error_deg"),
        "target_readback_match_ratio": match_ratio,
        "protocol_sha256": _protocol_digest(protocol_path),
        "manifest": str(manifest_path),
    }


def evaluate_bundle(manifest_paths: Iterable[Path | str]) -> dict:
    try:
        evaluations = [evaluate_manifest(path) for path in manifest_paths]
    except EvaluationError as exc:
        return {
            "schema_version": "physanim-product-bundle-evaluation/v1",
            "status": "INVALID",
            "fixture_authority": "DERIVED_ONLY",
            "failed_criteria": ["invalid_run"],
            "error": str(exc),
            "runs": [],
        }

    required = {"Normal": 3, "ZeroActions": 3, "DropControlDispatch": 1, "ForcedSupportLoss": 1}
    grouped = {variant: [item for item in evaluations if item["variant"] == variant] for variant in required}
    failed: list[str] = []
    if any(len(grouped[variant]) != count for variant, count in required.items()):
        failed.append("required_repetitions")
    for variant in ("Normal", "ZeroActions"):
        repetitions = [item["repetition"] for item in grouped[variant]]
        if len(repetitions) != len(set(repetitions)):
            failed.append(f"{variant.lower()}_duplicate_repetition")
    if grouped["Normal"] and not all(item["status"] == "PASS" for item in grouped["Normal"]):
        failed.append("normal_absolute_acceptance")
    if grouped["DropControlDispatch"]:
        drop = grouped["DropControlDispatch"][0]
        if drop["status"] != "FAIL" or "target_readback" not in drop["failed_criteria"]:
            failed.append("drop_dispatch_control")
    if grouped["ForcedSupportLoss"]:
        support = grouped["ForcedSupportLoss"][0]
        if support["status"] != "FAIL" or not ({"runtime_state", "support_gap"} & set(support["failed_criteria"])):
            failed.append("forced_support_control")

    ratio = None
    if grouped["Normal"] and grouped["ZeroActions"]:
        normal_auc = statistics.median(item["recovery_auc"] for item in grouped["Normal"])
        zero_auc = statistics.median(item["recovery_auc"] for item in grouped["ZeroActions"])
        ratio = normal_auc / zero_auc if zero_auc > 0.0 else math.inf
        if ratio > 0.8:
            failed.append("causal_recovery_advantage")

    authority = "PRODUCT_RUN" if evaluations and all(item["fixture_authority"] == "PRODUCT_RUN" for item in evaluations) else "EVALUATOR_UNIT_ONLY"
    status = "INVALID" if "required_repetitions" in failed else ("PASS" if not failed else "FAIL")
    return {
        "schema_version": "physanim-product-bundle-evaluation/v1",
        "status": status,
        "fixture_authority": authority,
        "failed_criteria": failed,
        "normal_to_zero_recovery_auc_ratio": ratio,
        "runs": evaluations,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Evaluate causal-standing raw run manifests")
    selection = parser.add_mutually_exclusive_group(required=True)
    selection.add_argument("--manifest", type=Path)
    selection.add_argument("--bundle", nargs="+", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    try:
        result = evaluate_manifest(args.manifest) if args.manifest else evaluate_bundle(args.bundle)
    except EvaluationError as exc:
        result = {
            "schema_version": "physanim-product-evaluation/v1",
            "status": "INVALID",
            "fixture_authority": "DERIVED_ONLY",
            "failed_criteria": ["invalid_run"],
            "error": str(exc),
        }
    serialized = json.dumps(result, indent=2, sort_keys=True)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(serialized + "\n", encoding="utf-8")
    print(serialized)
    return {"PASS": 0, "FAIL": 1, "INVALID": 2, "BLOCKED": 3}[result["status"]]


if __name__ == "__main__":
    sys.exit(main())

from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
from pathlib import Path
from typing import Callable, Iterable


class EvaluationError(ValueError):
    pass


EXPECTED_PROTOCOL = (
    Path(__file__).resolve().parents[1]
    / "product-gates"
    / "standing-plant-ladder.v2.json"
).resolve()


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
            raise EvaluationError(
                f"{label} line {line_number} is not valid JSON"
            ) from exc
        if not isinstance(row, dict):
            raise EvaluationError(f"{label} line {line_number} must be a JSON object")
        rows.append(row)
    return rows


def _require_fields(value: dict, fields: Iterable[str], label: str) -> None:
    missing = sorted(field for field in fields if field not in value)
    if missing:
        raise EvaluationError(
            f"{label} is missing required fields: {', '.join(missing)}"
        )


def _finite_number(value: object) -> bool:
    return (
        not isinstance(value, bool)
        and isinstance(value, (int, float))
        and math.isfinite(float(value))
    )


def _validate_monotonic(rows: list[dict], label: str) -> None:
    previous_sequence: int | None = None
    previous_time: float | None = None
    for index, row in enumerate(rows):
        sequence = row.get("sequence")
        time_sec = row.get("time_sec")
        if isinstance(sequence, bool) or not isinstance(sequence, int):
            raise EvaluationError(f"{label} row {index} sequence must be an integer")
        if not _finite_number(time_sec):
            raise EvaluationError(f"{label} row {index} time_sec must be finite")
        if previous_sequence is not None and sequence <= previous_sequence:
            raise EvaluationError(f"{label} sequence must be strictly increasing")
        current_time = float(time_sec)
        if previous_time is not None and current_time <= previous_time:
            raise EvaluationError(f"{label} time_sec must be strictly increasing")
        previous_sequence = sequence
        previous_time = current_time


def _validate_raw_types(rows: list[dict], fields: Iterable[str], label: str) -> None:
    bool_fields = {
        "root_is_simulating",
        "full_simulation_committed",
        "cmc_active",
        "cmc_tick_enabled",
        "cmc_updated_component_is_null",
        "inference_attempted",
        "inference_succeeded",
    }
    string_fields = {"runtime_state"}
    for index, row in enumerate(rows):
        for field in fields:
            value = row[field]
            if field in bool_fields:
                if not isinstance(value, bool):
                    raise EvaluationError(
                        f"{label} row {index} {field} must be a boolean"
                    )
            elif field in string_fields:
                if not isinstance(value, str) or not value:
                    raise EvaluationError(
                        f"{label} row {index} {field} must be a nonempty string"
                    )
            elif not _finite_number(value):
                raise EvaluationError(
                    f"{label} row {index} {field} must be finite numeric data"
                )


def _all(rows: list[dict], predicate: Callable[[dict], bool]) -> bool:
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
            "capture_window_sec",
            "perturbation_time_sec",
            "physics_samples",
            "policy_samples",
        ),
        "run manifest",
    )
    if manifest["schema_version"] != "physanim-development-run/v1":
        raise EvaluationError("run manifest schema_version is unsupported")
    if manifest["fixture_authority"] not in {
        "EVALUATOR_UNIT_ONLY",
        "DEVELOPMENT_GATE_RUN",
    }:
        raise EvaluationError("run manifest fixture_authority is unsupported")

    protocol_path = Path(manifest["protocol_path"]).resolve()
    if protocol_path != EXPECTED_PROTOCOL:
        raise EvaluationError("run manifest references the wrong standing-plant protocol")
    protocol = _read_json(protocol_path, "standing-plant protocol")
    if (
        protocol.get("schema_version") != "physanim-development-protocol/v1"
        or protocol.get("status") != "LOCKED"
        or protocol.get("authority") != "DEVELOPMENT_GATE_ONLY"
        or protocol.get("protocol_id") != "standing-plant-ladder"
        or protocol.get("version") != 2
    ):
        raise EvaluationError("run manifest references the wrong standing-plant protocol")

    layer = manifest["variant"]
    if layer not in protocol["ordered_layers"]:
        raise EvaluationError(f"unknown standing-plant layer: {layer}")
    if not isinstance(manifest["repetition"], int) or manifest["repetition"] < 1:
        raise EvaluationError("run repetition must be a positive integer")
    if not isinstance(manifest["source_commit"], str) or len(manifest["source_commit"]) != 40:
        raise EvaluationError("source_commit must be a 40-character commit")
    if not isinstance(manifest["source_tree_dirty"], bool):
        raise EvaluationError("source_tree_dirty must be a boolean")
    if (
        not isinstance(manifest["model_onnx_sha256"], str)
        or len(manifest["model_onnx_sha256"]) != 64
    ):
        raise EvaluationError("model_onnx_sha256 must be a SHA-256 digest")

    layer_contract = protocol["layers"][layer]
    expected_window = float(layer_contract["capture_window_sec"])
    if (
        not _finite_number(manifest["capture_window_sec"])
        or not math.isclose(
            float(manifest["capture_window_sec"]), expected_window, abs_tol=1.0e-9
        )
    ):
        raise EvaluationError("capture_window_sec does not match the locked layer")
    if not _finite_number(manifest["standing_window_start_sec"]):
        raise EvaluationError("standing_window_start_sec must be finite")
    if not _finite_number(manifest["reference_pelvis_height_cm"]):
        raise EvaluationError("reference_pelvis_height_cm must be finite")

    run_dir = manifest_path.parent
    physics_path = (run_dir / manifest["physics_samples"]).resolve()
    policy_path = (run_dir / manifest["policy_samples"]).resolve()
    for candidate, label in (
        (physics_path, "physics samples"),
        (policy_path, "policy samples"),
    ):
        if run_dir not in candidate.parents:
            raise EvaluationError(f"{label} must stay inside the run directory")

    physics_rows = _read_jsonl(physics_path, "physics samples")
    policy_rows = _read_jsonl(policy_path, "policy samples", allow_empty=True)
    _validate_monotonic(physics_rows, "physics samples")
    _validate_monotonic(policy_rows, "policy samples")

    physics_fields = protocol["sample_streams"]["physics"]["required_fields"]
    policy_fields = protocol["sample_streams"]["policy"]["required_fields"]
    for index, row in enumerate(physics_rows):
        _require_fields(row, physics_fields, f"physics row {index}")
    for index, row in enumerate(policy_rows):
        _require_fields(row, policy_fields, f"policy row {index}")
    _validate_raw_types(physics_rows, physics_fields, "physics samples")
    _validate_raw_types(policy_rows, policy_fields, "policy samples")

    start = float(manifest["standing_window_start_sec"])
    end = start + expected_window
    physics_window = [
        row for row in physics_rows if start <= float(row["time_sec"]) <= end
    ]
    policy_window = [
        row for row in policy_rows if start <= float(row["time_sec"]) < end
    ]
    if (
        len(physics_window) < 2
        or float(physics_window[0]["time_sec"]) > start
        or float(physics_window[-1]["time_sec"]) < end
    ):
        raise EvaluationError("physics stream does not cover the locked capture window")

    failed: list[str] = []

    def require(name: str, condition: bool) -> None:
        if not condition:
            failed.append(name)

    invariants = protocol["invariants"]
    required_bodies = int(invariants["required_body_count"])
    required_controls = int(invariants["required_control_count"])
    require(
        "body_topology",
        _all(
            physics_window,
            lambda row: int(row["body_valid_count"]) == required_bodies
            and int(row["body_simulating_count"]) == required_bodies,
        ),
    )
    require(
        "control_gain_readback",
        _all(
            physics_window,
            lambda row: int(row["control_gain_match_count"]) == required_controls,
        ),
    )
    require(
        "full_simulation_committed",
        _all(
            physics_window,
            lambda row: row["full_simulation_committed"] is True,
        ),
    )
    require(
        "root_simulating",
        _all(physics_window, lambda row: row["root_is_simulating"] is True),
    )
    require(
        "cmc_inactive",
        _all(
            physics_window,
            lambda row: row["cmc_active"] is False
            and row["cmc_tick_enabled"] is False
            and row["cmc_updated_component_is_null"] is True,
        ),
    )
    require(
        "capsule_collision",
        _all(
            physics_window,
            lambda row: int(row["capsule_collision_enabled"]) == 0,
        ),
    )
    require(
        "movement_reclaim",
        _all(
            physics_window,
            lambda row: int(row["movement_reclaim_count"]) == 0,
        ),
    )
    require(
        "shell_helper",
        _all(
            physics_window,
            lambda row: int(row["shell_helper_used_count"]) == 0,
        ),
    )
    require(
        "topology_change",
        _all(
            physics_window,
            lambda row: int(row["topology_change_count"]) == 0,
        ),
    )

    speed_limits = (
        (
            "root_linear_speed",
            "root_linear_speed_cm_per_sec",
            "maximum_root_linear_speed_cm_per_sec",
        ),
        (
            "root_angular_speed",
            "root_angular_speed_deg_per_sec",
            "maximum_root_angular_speed_deg_per_sec",
        ),
        (
            "body_linear_speed",
            "max_body_linear_speed_cm_per_sec",
            "maximum_body_linear_speed_cm_per_sec",
        ),
        (
            "body_angular_speed",
            "max_body_angular_speed_deg_per_sec",
            "maximum_body_angular_speed_deg_per_sec",
        ),
    )
    for criterion, field, limit_name in speed_limits:
        limit = float(invariants[limit_name])
        require(
            criterion,
            _all(physics_window, lambda row, f=field, cap=limit: 0.0 <= float(row[f]) <= cap),
        )

    hold = protocol["hold_acceptance"]
    if layer in hold["applies_to_layers"]:
        reference_height = float(manifest["reference_pelvis_height_cm"])
        require(
            "reference_pelvis_height",
            reference_height > 0.0 and math.isfinite(reference_height),
        )
        require(
            "runtime_state",
            _all(
                physics_window,
                lambda row: row["runtime_state"] == hold["required_runtime_state"],
            ),
        )
        require(
            "pelvis_height",
            _all(
                physics_window,
                lambda row: float(row["pelvis_height_cm"])
                >= reference_height * float(hold["minimum_pelvis_height_ratio"]),
            ),
        )
        require(
            "root_tilt",
            _all(
                physics_window,
                lambda row: 0.0
                <= float(row["root_tilt_deg"])
                <= float(hold["maximum_root_tilt_deg"]),
            ),
        )
        require(
            "penetration",
            _all(
                physics_window,
                lambda row: 0.0
                <= float(row["max_penetration_cm"])
                <= float(hold["maximum_penetration_cm"]),
            ),
        )
        require(
            "support_gap",
            _all(
                physics_window,
                lambda row: 0.0
                <= float(row["support_gap_ms"])
                <= float(hold["maximum_support_gap_ms"]),
            ),
        )

    policy_contract = protocol["sample_streams"]["policy"]
    policy_required = layer in policy_contract["required_for_layers"]
    match_ratio: float | None = None
    if policy_required:
        expected_steps = expected_window * float(policy_contract["expected_rate_hz"])
        require(
            "policy_step_coverage",
            len(policy_window)
            >= math.floor(expected_steps * float(policy_contract["minimum_step_coverage"])),
        )
        require(
            "inference",
            _all(
                policy_window,
                lambda row: row["inference_attempted"] is True
                and row["inference_succeeded"] is True,
            ),
        )
        if layer == "ZeroActions":
            require(
                "zero_action_conditioning",
                _all(
                    policy_window,
                    lambda row: abs(float(row["conditioned_action_l2"])) <= 1.0e-9,
                ),
            )
        elif layer == "RealOnnxPolicy":
            require(
                "real_policy_action",
                any(float(row["conditioned_action_l2"]) > 1.0e-9 for row in policy_window),
            )

        attempted_writes = sum(
            int(row["target_write_attempt_count"]) for row in policy_window
        )
        matched_writes = sum(
            int(row["target_readback_match_count"]) for row in policy_window
        )
        match_ratio = (
            matched_writes / attempted_writes if attempted_writes > 0 else 0.0
        )
        maximum_error = max(
            (
                float(row["target_readback_max_error_deg"])
                for row in policy_window
            ),
            default=math.inf,
        )
        require(
            "target_readback",
            attempted_writes > 0
            and match_ratio
            >= float(policy_contract["minimum_target_readback_match_ratio"])
            and maximum_error
            <= float(policy_contract["maximum_target_readback_error_deg"]),
        )
    else:
        require(
            "policy_disabled",
            all(
                row["inference_attempted"] is False
                and row["inference_succeeded"] is False
                and abs(float(row["raw_action_l2"])) <= 1.0e-9
                and abs(float(row["conditioned_action_l2"])) <= 1.0e-9
                and int(row["target_write_attempt_count"]) == 0
                and int(row["target_readback_match_count"]) == 0
                for row in policy_window
            ),
        )

    return {
        "schema_version": "physanim-development-evaluation/v1",
        "run_id": manifest["run_id"],
        "layer": layer,
        "repetition": manifest["repetition"],
        "status": "PASS" if not failed else "FAIL",
        "fixture_authority": manifest["fixture_authority"],
        "failed_criteria": failed,
        "target_readback_match_ratio": match_ratio,
        "protocol_sha256": _protocol_digest(protocol_path),
        "manifest": str(manifest_path),
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Evaluate one raw standing-plant development run"
    )
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    try:
        result = evaluate_manifest(args.manifest)
    except EvaluationError as exc:
        result = {
            "schema_version": "physanim-development-evaluation/v1",
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

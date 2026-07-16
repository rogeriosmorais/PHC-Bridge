from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
from typing import Any, Iterable

from scripts.locomotion_causal_metrics import analyze_trace, evaluate_metric_contract


class ProtocolLinkageError(ValueError):
    pass


def _read_json(path: Path, label: str) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise ProtocolLinkageError(f"{label} does not exist: {path}") from exc
    except json.JSONDecodeError as exc:
        raise ProtocolLinkageError(f"{label} is invalid JSON: {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise ProtocolLinkageError(f"{label} must be a JSON object: {path}")
    return value


def _read_jsonl(path: Path, label: str) -> list[dict[str, Any]]:
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except FileNotFoundError as exc:
        raise ProtocolLinkageError(f"{label} does not exist: {path}") from exc
    if not lines:
        raise ProtocolLinkageError(f"{label} is empty: {path}")
    rows: list[dict[str, Any]] = []
    for line_number, line in enumerate(lines, start=1):
        try:
            value = json.loads(line)
        except json.JSONDecodeError as exc:
            raise ProtocolLinkageError(f"{label} line {line_number} is invalid JSON") from exc
        if not isinstance(value, dict):
            raise ProtocolLinkageError(f"{label} line {line_number} must be an object")
        rows.append(value)
    return rows


def _require_fields(value: dict[str, Any], fields: Iterable[str], label: str) -> None:
    missing = sorted(field for field in fields if field not in value)
    if missing:
        raise ProtocolLinkageError(f"{label} is missing fields: {', '.join(missing)}")


def _assert_exact(actual: Any, expected: Any, label: str) -> None:
    if actual != expected:
        raise ProtocolLinkageError(f"{label} does not match the loaded protocol: actual={actual!r}, expected={expected!r}")


def _assert_close(actual: Any, expected: Any, label: str, *, tolerance: float = 1.0e-6) -> None:
    if isinstance(actual, bool) or not isinstance(actual, (int, float)):
        raise ProtocolLinkageError(f"{label} must be numeric")
    actual_value = float(actual)
    expected_value = float(expected)
    if not math.isfinite(actual_value) or abs(actual_value - expected_value) > tolerance:
        raise ProtocolLinkageError(
            f"{label} does not match the loaded protocol: actual={actual_value}, expected={expected_value}"
        )


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def _resolve_step(protocol: dict[str, Any], time_sec: float) -> tuple[str, float, bool]:
    script = protocol["script"]
    acceleration = script["acceleration"]
    cruise = script["cruise"]
    moving_turn = script["moving_turn"]
    deceleration = script["deceleration"]

    if time_sec < float(acceleration["start_sec"]):
        return "StandingHold", 0.0, False
    if time_sec < float(acceleration["end_sec"]):
        duration = float(acceleration["end_sec"]) - float(acceleration["start_sec"])
        alpha = (time_sec - float(acceleration["start_sec"])) / duration
        intent = float(acceleration["intent_start"]) + alpha * (
            float(acceleration["intent_end"]) - float(acceleration["intent_start"])
        )
        return "Acceleration", intent, True
    if time_sec < float(cruise["end_sec"]):
        return "Cruise", float(cruise["intent"]), True
    if time_sec < float(moving_turn["end_sec"]):
        return "MovingTurn", float(moving_turn["intent"]), True
    if time_sec < float(deceleration["end_sec"]):
        duration = float(deceleration["end_sec"]) - float(deceleration["start_sec"])
        alpha = (time_sec - float(deceleration["start_sec"])) / duration
        intent = float(deceleration["intent_start"]) + alpha * (
            float(deceleration["intent_end"]) - float(deceleration["intent_start"])
        )
        return "Deceleration", intent, True
    return "Settle", 0.0, False


def _validate_protocol_linkage_impl(
    manifest_path: Path | str,
    *,
    require_complete_streams: bool,
) -> dict[str, Any]:
    manifest_path = Path(manifest_path).resolve()
    manifest = _read_json(manifest_path, "run manifest")
    _require_fields(
        manifest,
        (
            "schema_version",
            "fixture_authority",
            "protocol_path",
            "protocol_sha256",
            "protocol_schema_version",
            "protocol_id",
            "protocol_version",
            "protocol_status",
            "protocol_map",
            "protocol_actor_class",
            "protocol_skeleton",
            "protocol_model_asset",
            "protocol_test_family",
            "protocol_renderer_mode",
            "resolved_script",
            "resolved_acceptance",
            "resolved_stability_cost",
            "resolved_physics_minimum_samples",
            "resolved_policy_minimum_samples",
            "variant",
            "repetition",
            "physics_samples",
            "policy_samples",
            "scenario_summary",
            "scripted_locomotion_run",
            "human_input",
            "root_authority",
            "motion_source",
        ),
        "run manifest",
    )
    _assert_exact(manifest["schema_version"], "physanim-scripted-locomotion-run/v2", "manifest schema_version")
    _assert_exact(manifest["fixture_authority"], "PRODUCT_RUN", "fixture authority")
    _assert_exact(manifest["scripted_locomotion_run"], True, "scripted_locomotion_run")
    _assert_exact(manifest["human_input"], False, "human_input")

    protocol_path = Path(str(manifest["protocol_path"])).resolve()
    protocol = _read_json(protocol_path, "scripted-locomotion protocol")
    protocol_sha = _sha256(protocol_path)
    _assert_exact(protocol.get("schema_version"), "physanim-product-protocol/v1", "protocol schema_version")
    _assert_exact(protocol.get("protocol_id"), "scripted-causal-locomotion", "protocol_id")
    _assert_exact(protocol.get("status"), "LOCKED", "protocol status")
    _assert_exact(manifest["protocol_sha256"], protocol_sha, "protocol SHA-256")
    _assert_exact(manifest["protocol_schema_version"], protocol["schema_version"], "manifest protocol schema")
    _assert_exact(manifest["protocol_id"], protocol["protocol_id"], "manifest protocol id")
    _assert_exact(manifest["protocol_version"], protocol["version"], "manifest protocol version")
    _assert_exact(manifest["protocol_status"], protocol["status"], "manifest protocol status")
    _assert_exact(manifest["protocol_map"], protocol["map"], "protocol map")
    _assert_exact(manifest["protocol_actor_class"], protocol["actor_class"], "protocol actor class")
    _assert_exact(manifest["protocol_skeleton"], protocol["skeleton"], "protocol skeleton")
    _assert_exact(manifest["protocol_model_asset"], protocol["model_asset"], "protocol model asset")
    _assert_exact(manifest["protocol_test_family"], protocol["test_family"], "protocol test family")
    _assert_exact(manifest["protocol_renderer_mode"], protocol["renderer_mode"], "protocol renderer mode")
    _assert_exact(manifest["root_authority"], protocol["root_authority"], "root authority")
    _assert_exact(manifest["motion_source"], protocol["motion_source"], "motion source")
    _assert_exact(manifest["variant"] in protocol["variants"], True, "variant declaration")
    repetition_limit = protocol["repetitions"].get(manifest["variant"])
    if not isinstance(manifest["repetition"], int) or manifest["repetition"] < 1:
        raise ProtocolLinkageError("manifest repetition must be a positive integer")
    if not isinstance(repetition_limit, int) or manifest["repetition"] > repetition_limit:
        raise ProtocolLinkageError("manifest repetition exceeds the loaded protocol")

    _assert_exact(manifest["resolved_script"], protocol["script"], "resolved script")
    _assert_exact(manifest["resolved_acceptance"], protocol["acceptance"], "resolved acceptance")
    _assert_exact(manifest["resolved_stability_cost"], protocol["stability_cost"], "resolved stability cost")
    _assert_exact(
        manifest["resolved_physics_minimum_samples"],
        protocol["sample_streams"]["physics"]["minimum_samples"],
        "physics minimum sample count",
    )
    _assert_exact(
        manifest["resolved_policy_minimum_samples"],
        protocol["sample_streams"]["policy"]["minimum_samples"],
        "policy minimum sample count",
    )

    run_dir = manifest_path.parent
    physics_path = (run_dir / str(manifest["physics_samples"])).resolve()
    policy_path = (run_dir / str(manifest["policy_samples"])).resolve()
    summary_path = (run_dir / str(manifest["scenario_summary"])).resolve()
    for candidate, label in (
        (physics_path, "physics samples"),
        (policy_path, "policy samples"),
        (summary_path, "scenario summary"),
    ):
        if run_dir not in candidate.parents:
            raise ProtocolLinkageError(f"{label} must remain inside the run directory")

    physics_rows = _read_jsonl(physics_path, "physics samples")
    policy_rows = _read_jsonl(policy_path, "policy samples")
    summary = _read_json(summary_path, "scenario summary")
    _assert_exact(summary.get("schema_version"), "physanim-scripted-locomotion-summary/v2", "summary schema")
    _assert_exact(summary.get("protocol_id"), protocol["protocol_id"], "summary protocol id")
    _assert_exact(summary.get("protocol_version"), protocol["version"], "summary protocol version")
    _assert_exact(summary.get("protocol_sha256"), protocol_sha, "summary protocol SHA-256")
    _assert_close(
        summary.get("nominal_speed_cm_per_sec"),
        protocol["script"]["nominal_speed_cm_per_sec"],
        "summary nominal speed",
    )

    physics_required = protocol["sample_streams"]["physics"]["required_fields"]
    policy_required = protocol["sample_streams"]["policy"]["required_fields"]
    if require_complete_streams:
        if len(physics_rows) < int(protocol["sample_streams"]["physics"]["minimum_samples"]):
            raise ProtocolLinkageError("physics stream is shorter than the protocol minimum")
        if len(policy_rows) < int(protocol["sample_streams"]["policy"]["minimum_samples"]):
            raise ProtocolLinkageError("policy stream is shorter than the protocol minimum")

    fixed_delta = float(protocol["script"]["fixed_delta_time_sec"])
    nominal_speed = float(protocol["script"]["nominal_speed_cm_per_sec"])
    previous_time: float | None = None
    for index, row in enumerate(physics_rows):
        _require_fields(row, physics_required, f"physics row {index}")
        time_sec = float(row["time_sec"])
        if previous_time is not None:
            _assert_close(time_sec - previous_time, fixed_delta, f"physics cadence row {index}", tolerance=2.0e-5)
        previous_time = time_sec
        expected_phase, expected_intent, expected_move = _resolve_step(protocol, time_sec)
        _assert_exact(row["script_phase"], expected_phase, f"physics phase row {index}")
        _assert_close(row["script_intent_magnitude"], expected_intent, f"physics intent row {index}", tolerance=2.0e-4)
        _assert_exact(row["human_input"], False, f"physics human input row {index}")
        expected_conditioning = expected_move and manifest["variant"] != "DropTrajectoryConditioning"
        _assert_exact(
            row["trajectory_conditioning_published"],
            expected_conditioning,
            f"physics trajectory conditioning row {index}",
        )
        shell_speed = float(row["shell_accepted_speed_cm_per_sec"])
        if shell_speed < -1.0e-6 or shell_speed > nominal_speed + 1.0e-3:
            raise ProtocolLinkageError(f"physics shell speed row {index} exceeds protocol nominal speed")

    for index, row in enumerate(policy_rows):
        _require_fields(row, policy_required, f"policy row {index}")
        time_sec = float(row["time_sec"])
        expected_phase, _, expected_move = _resolve_step(protocol, time_sec)
        _assert_exact(row["script_phase"], expected_phase, f"policy phase row {index}")
        expected_conditioning = expected_move and manifest["variant"] != "DropTrajectoryConditioning"
        _assert_exact(
            row["trajectory_conditioning_published"],
            expected_conditioning,
            f"policy trajectory conditioning row {index}",
        )

    return {
        "valid": True,
        "complete_streams_required": require_complete_streams,
        "manifest_path": str(manifest_path),
        "protocol_path": str(protocol_path),
        "protocol_sha256": protocol_sha,
        "protocol_id": protocol["protocol_id"],
        "protocol_version": protocol["version"],
        "variant": manifest["variant"],
        "repetition": manifest["repetition"],
        "physics_samples": len(physics_rows),
        "policy_samples": len(policy_rows),
    }


def validate_protocol_identity_and_observed_schedule(manifest_path: Path | str) -> dict[str, Any]:
    return _validate_protocol_linkage_impl(manifest_path, require_complete_streams=False)


def validate_protocol_linkage(manifest_path: Path | str) -> dict[str, Any]:
    return _validate_protocol_linkage_impl(manifest_path, require_complete_streams=True)


def evaluate_protocol_causal_metrics(manifest_path: Path | str) -> dict[str, Any]:
    manifest_path = Path(manifest_path).resolve()
    linkage = validate_protocol_linkage(manifest_path)
    manifest = _read_json(manifest_path, "run manifest")
    protocol_path = Path(str(manifest["protocol_path"])).resolve()
    protocol = _read_json(protocol_path, "scripted-locomotion protocol")
    physics_path = (manifest_path.parent / str(manifest["physics_samples"])).resolve()
    physics_rows = _read_jsonl(physics_path, "physics samples")
    metrics = analyze_trace(physics_rows)

    contract = protocol.get("causal_metrics")
    if contract is None:
        return {
            "schema_version": "physanim-scripted-locomotion-causal-evaluation/v1",
            "verdict": "NOT_APPLICABLE",
            "reason": "The loaded protocol does not declare a causal_metrics contract.",
            "linkage": linkage,
            "metrics": metrics,
        }
    if not isinstance(contract, dict):
        raise ProtocolLinkageError("protocol causal_metrics must be an object")
    variants = contract.get("apply_to_variants")
    if not isinstance(variants, list) or not all(isinstance(item, str) and item for item in variants):
        raise ProtocolLinkageError("protocol causal_metrics.apply_to_variants must be an array of variants")
    if manifest["variant"] not in variants:
        return {
            "schema_version": "physanim-scripted-locomotion-causal-evaluation/v1",
            "verdict": "NOT_APPLICABLE",
            "reason": f"Variant {manifest['variant']} is outside causal_metrics.apply_to_variants.",
            "linkage": linkage,
            "metrics": metrics,
        }

    evaluation = evaluate_metric_contract(metrics, contract)
    return {
        "schema_version": "physanim-scripted-locomotion-causal-evaluation/v1",
        "verdict": evaluation["verdict"],
        "linkage": linkage,
        "contract": contract,
        "failed_criteria": evaluation["failed_criteria"],
        "criteria": evaluation["criteria"],
        "metrics": metrics,
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate authoritative protocol linkage for one scripted-locomotion product run."
    )
    parser.add_argument("manifest", type=Path)
    parser.add_argument(
        "--evaluate-causal",
        action="store_true",
        help="Apply the loaded protocol's causal_metrics contract and fail on a behavioral rejection.",
    )
    args = parser.parse_args()
    try:
        identity_result = validate_protocol_identity_and_observed_schedule(args.manifest)
    except ProtocolLinkageError as exc:
        print(
            json.dumps(
                {
                    "protocol_linkage_valid": False,
                    "evidence_valid": False,
                    "error": str(exc),
                },
                indent=2,
            )
        )
        return 1
    try:
        complete_result = validate_protocol_linkage(args.manifest)
    except ProtocolLinkageError as exc:
        print(
            json.dumps(
                {
                    "protocol_linkage_valid": True,
                    "evidence_valid": False,
                    "protocol": identity_result,
                    "error": str(exc),
                },
                indent=2,
            )
        )
        return 1
    output: dict[str, Any] = {
        "protocol_linkage_valid": True,
        "evidence_valid": True,
        "protocol": complete_result,
    }
    if args.evaluate_causal:
        try:
            causal_result = evaluate_protocol_causal_metrics(args.manifest)
        except (ProtocolLinkageError, ValueError) as exc:
            output["causal_evaluation"] = {"verdict": "INVALID", "error": str(exc)}
            print(json.dumps(output, indent=2))
            return 1
        output["causal_evaluation"] = causal_result
        print(json.dumps(output, indent=2))
        return 0 if causal_result["verdict"] in {"PASS", "NOT_APPLICABLE"} else 2
    print(json.dumps(output, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

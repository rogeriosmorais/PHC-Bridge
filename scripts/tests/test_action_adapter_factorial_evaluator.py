from __future__ import annotations

import json
from pathlib import Path

import pytest

from scripts.evaluate_action_adapter_factorial import (
    ActionAdapterFactorialError,
    evaluate_factorial,
    main,
)
from scripts.tests.test_action_axis_frame_experiment_evaluator import (
    COMPONENT_MODE,
    WORLD_MODE,
    _angle,
    _axis_angle,
    _load,
    _make_v2_arm,
    _mul,
    _refresh_v2,
    _store,
)


def _make_run(path: Path, mode: str, bind_neutral: bool) -> Path:
    path.mkdir(parents=True)
    trace_path = _make_v2_arm(path / "manny-local-frame-roundtrip.json", mode)
    trace = _load(trace_path)
    for entry in trace["controls"]:
        bind = entry["action_bind_parent_relative_rotation_xyzw"]
        neutral = bind if bind_neutral else _mul(_axis_angle(0, 5.0), bind)
        entry["policy_neutral_parent_relative_rotation_xyzw"] = neutral
        entry[
            "policy_neutral_vs_action_bind_parent_relative_angular_delta_degrees"
        ] = _angle(neutral, bind)
        entry[
            "policy_neutral_vs_observation_bind_parent_relative_angular_delta_degrees"
        ] = _angle(neutral, entry["observation_bind_parent_relative_rotation_xyzw"])
        _refresh_v2(entry, mode)
    _store(trace_path, trace)
    snapshot = {
        "schema_version": "physanim-policy-input-snapshot/v1",
        "captured": True,
        "self_observation_width": 1,
        "mimic_target_poses_width": 1,
        "terrain_width": 1,
        "action_width": 1,
        "self_observation": [1.0],
        "mimic_target_poses": [2.0],
        "terrain": [3.0],
        "actions": [4.0],
    }
    (path / "policy-input-snapshot.json").write_text(json.dumps(snapshot))
    policy_row = {
        "sequence": 0,
        "raw_actions": [0.1, -0.2, 0.3],
        "conditioned_actions": [0.1, -0.2, 0.3],
        "target_write_attempt_count": 21,
        "target_readback_match_count": 21,
    }
    (path / "policy.jsonl").write_text(json.dumps(policy_row) + "\n")
    manifest = {
        "source_commit": "abc123",
        "source_tree_dirty": False,
        "model_onnx_sha256": "model",
        "protocol_path": "protocol.json",
        "variant": "RealOnnxPolicy",
        "reference_pelvis_height_cm": 91.2,
        "capture_window_sec": 10,
    }
    (path / "manifest.json").write_text(json.dumps(manifest))
    return path


def _make_factorial(tmp_path: Path) -> tuple[Path, Path, Path, Path]:
    return (
        _make_run(tmp_path / "a", WORLD_MODE, False),
        _make_run(tmp_path / "b", COMPONENT_MODE, False),
        _make_run(tmp_path / "c", WORLD_MODE, True),
        _make_run(tmp_path / "d", COMPONENT_MODE, True),
    )


def test_factorial_supports_exact_inverse_pattern(tmp_path: Path) -> None:
    result = evaluate_factorial(*_make_factorial(tmp_path))

    assert result["status"] == "VALID"
    assert result["hypothesis_verdict"] == "SUPPORTED"
    assert result["product_success"] is False
    assert result["first_policy_input_exact"] is True
    assert result["first_policy_output_exact"] is True
    assert all(result["checks"].values())
    assert result["metrics"]["D_component_bind"]["maximum_probe_error_degrees"] <= 1.0e-3


def test_first_policy_input_mismatch_is_invalid(tmp_path: Path) -> None:
    arms = _make_factorial(tmp_path)
    snapshot = json.loads((arms[3] / "policy-input-snapshot.json").read_text())
    snapshot["self_observation"] = [9.0]
    (arms[3] / "policy-input-snapshot.json").write_text(json.dumps(snapshot))

    with pytest.raises(ActionAdapterFactorialError, match="First-policy input"):
        evaluate_factorial(*arms)


def test_component_bind_that_keeps_captured_neutral_falsifies(tmp_path: Path) -> None:
    a, b, c, _ = _make_factorial(tmp_path)
    d = _make_run(tmp_path / "wrong-d", COMPONENT_MODE, False)

    result = evaluate_factorial(a, b, c, d)

    assert result["status"] == "VALID"
    assert result["hypothesis_verdict"] == "FALSIFIED"
    assert result["checks"]["D_is_exact_inverse"] is False


def test_cli_writes_invalid_result(tmp_path: Path) -> None:
    output = tmp_path / "result.json"
    exit_code = main(
        [
            "--arm-a", str(tmp_path / "missing-a"),
            "--arm-b", str(tmp_path / "missing-b"),
            "--arm-c", str(tmp_path / "missing-c"),
            "--arm-d", str(tmp_path / "missing-d"),
            "--output", str(output),
        ]
    )

    assert exit_code == 2
    result = json.loads(output.read_text())
    assert result["status"] == "INVALID"
    assert result["hypothesis_verdict"] == "NOT_EVALUATED"

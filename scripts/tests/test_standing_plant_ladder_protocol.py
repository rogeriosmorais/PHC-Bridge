import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PROTOCOL_V1 = ROOT / "product-gates" / "standing-plant-ladder.v1.json"
PROTOCOL = ROOT / "product-gates" / "standing-plant-ladder.v2.json"


def test_standing_plant_ladder_protocol_is_locked_and_ordered() -> None:
    protocol = json.loads(PROTOCOL.read_text(encoding="utf-8"))

    assert protocol["schema_version"] == "physanim-development-protocol/v1"
    assert protocol["protocol_id"] == "standing-plant-ladder"
    assert protocol["version"] == 2
    assert protocol["supersedes_version"] == 1
    assert protocol["status"] == "LOCKED"
    assert protocol["authority"] == "DEVELOPMENT_GATE_ONLY"
    assert protocol["ordered_layers"] == [
        "ControlsOff",
        "DampingOnly",
        "FixedNeutralTarget",
        "ZeroActions",
        "RealOnnxPolicy",
    ]
    assert list(protocol["layers"]) == protocol["ordered_layers"]
    assert set(protocol["outcomes"]) == {"PASS", "FAIL", "INVALID", "BLOCKED"}
    assert protocol["product_success"].startswith("Never")


def test_standing_plant_ladder_v1_remains_locked_and_unchanged() -> None:
    protocol = json.loads(PROTOCOL_V1.read_text(encoding="utf-8"))

    assert protocol["version"] == 1
    assert protocol["status"] == "LOCKED"
    assert "hold_acceptance" not in protocol


def test_standing_plant_ladder_reuses_locked_runtime_safety_limits() -> None:
    protocol = json.loads(PROTOCOL.read_text(encoding="utf-8"))
    invariants = protocol["invariants"]

    assert invariants["required_body_count"] == 22
    assert invariants["required_control_count"] == 21
    assert invariants["maximum_root_linear_speed_cm_per_sec"] == 1200.0
    assert invariants["maximum_root_angular_speed_deg_per_sec"] == 720.0
    assert invariants["maximum_body_linear_speed_cm_per_sec"] == 1000.0
    assert invariants["maximum_body_angular_speed_deg_per_sec"] == 4000.0


def test_standing_plant_ladder_modes_are_discriminative() -> None:
    protocol = json.loads(PROTOCOL.read_text(encoding="utf-8"))
    layers = protocol["layers"]

    assert layers["ControlsOff"]["control_damping"] == "zero"
    assert layers["DampingOnly"]["control_damping"] == "configured"
    assert layers["FixedNeutralTarget"]["target_dispatch"] == "captured_parent_relative_neutral"
    assert layers["ZeroActions"]["policy_inference"] == "required_then_zeroed"
    assert layers["RealOnnxPolicy"]["policy_inference"] == "required_existing_onnx"


def test_standing_layers_reuse_the_locked_causal_hold_contract() -> None:
    protocol = json.loads(PROTOCOL.read_text(encoding="utf-8"))
    source = protocol["hold_acceptance_source"]
    hold = protocol["hold_acceptance"]

    assert source == {
        "protocol": "product-gates/causal-standing.v1.json",
        "sha256": "75b29360907028d081cbfa43e965a35fef760873c76d71b52f2147e99f54606a",
    }
    assert hold["applies_to_layers"] == [
        "FixedNeutralTarget",
        "ZeroActions",
        "RealOnnxPolicy",
    ]
    assert hold["minimum_pelvis_height_ratio"] == 0.7
    assert hold["maximum_root_tilt_deg"] == 45.0
    assert hold["maximum_penetration_cm"] == 2.0
    assert hold["maximum_support_gap_ms"] == 100.0
    assert hold["required_runtime_state"] == "BalanceActive_Standing"


def test_standing_plant_ladder_requires_raw_topology_and_control_readback() -> None:
    protocol = json.loads(PROTOCOL.read_text(encoding="utf-8"))
    fields = set(protocol["sample_streams"]["physics"]["required_fields"])

    assert {
        "body_valid_count",
        "body_simulating_count",
        "control_gain_match_count",
        "full_simulation_committed",
        "root_linear_speed_cm_per_sec",
        "root_angular_speed_deg_per_sec",
        "max_body_linear_speed_cm_per_sec",
        "max_body_angular_speed_deg_per_sec",
    } <= fields

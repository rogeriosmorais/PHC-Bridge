import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PROTOCOL = ROOT / "product-gates" / "standing-plant-ladder.v1.json"


def test_standing_plant_ladder_protocol_is_locked_and_ordered() -> None:
    protocol = json.loads(PROTOCOL.read_text(encoding="utf-8"))

    assert protocol["schema_version"] == "physanim-development-protocol/v1"
    assert protocol["protocol_id"] == "standing-plant-ladder"
    assert protocol["version"] == 1
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

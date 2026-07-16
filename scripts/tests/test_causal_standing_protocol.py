import json
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
PROTOCOL_PATH = REPO_ROOT / "product-gates" / "causal-standing.v1.json"


def load_protocol() -> dict:
    return json.loads(PROTOCOL_PATH.read_text(encoding="utf-8"))


def test_causal_standing_protocol_is_locked_and_scoped_to_local_assets() -> None:
    protocol = load_protocol()

    assert protocol["schema_version"] == "physanim-product-protocol/v1"
    assert protocol["protocol_id"] == "causal-standing"
    assert protocol["version"] == 1
    assert protocol["status"] == "LOCKED"
    assert protocol["map"] == "/Game/ThirdPerson/Lvl_ThirdPerson"
    assert protocol["model_asset"] == "/Game/NNEModels/phc_policy.phc_policy"
    assert "F:\\NewEngine\\" not in json.dumps(protocol)


def test_protocol_defines_discriminative_variants_and_outcomes() -> None:
    protocol = load_protocol()

    assert protocol["variants"] == [
        "Normal",
        "ZeroActions",
        "DropControlDispatch",
        "ForcedSupportLoss",
    ]
    assert protocol["outcomes"] == ["PASS", "FAIL", "INVALID", "BLOCKED"]
    assert protocol["repetitions"] == {
        "Normal": 3,
        "ZeroActions": 3,
        "DropControlDispatch": 1,
        "ForcedSupportLoss": 1,
    }


def test_protocol_keeps_physics_and_policy_cadences_independent() -> None:
    protocol = load_protocol()
    streams = protocol["sample_streams"]

    assert streams["physics"]["clock"] == "game_tick"
    assert streams["policy"]["clock"] == "policy_step"
    assert streams["policy"]["expected_rate_hz"] == 30
    assert "sample_count_less_than_or_equal" not in json.dumps(protocol)


def test_protocol_freezes_absolute_and_comparative_acceptance() -> None:
    acceptance = load_protocol()["acceptance"]

    assert acceptance["startup_timeout_sec"] == 20.0
    assert acceptance["standing_window_sec"] == 10.0
    assert acceptance["baseline_window_sec"] == 2.0
    assert acceptance["perturbation_delta_v_cm_per_sec"] == 15.0
    assert acceptance["minimum_pelvis_height_ratio"] == 0.70
    assert acceptance["maximum_root_tilt_deg"] == 45.0
    assert acceptance["maximum_penetration_cm"] == 2.0
    assert acceptance["maximum_support_gap_ms"] == 100.0
    assert acceptance["minimum_policy_step_coverage"] == 0.90
    assert acceptance["normal_to_zero_recovery_auc_ratio_max"] == 0.80

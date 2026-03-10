"""Tests for the locked Stage 1 ONNX export path."""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
import onnx
import pytest

try:
    import onnxruntime as ort
except ImportError:  # pragma: no cover - dependency is installed in the locked venv
    ort = None


REPO_ROOT = Path(__file__).resolve().parents[2]
TRAINING_ROOT = REPO_ROOT / "Training"
if str(TRAINING_ROOT) not in sys.path:
    sys.path.insert(0, str(TRAINING_ROOT))

from physanim.export_onnx import (  # noqa: E402
    DEFAULT_CHECKPOINT_PATH,
    DEFAULT_OUTPUT_PATH,
    DEFAULT_UE_IMPORT_PATH,
    Stage1PolicySpec,
    compare_pytorch_and_onnx,
    export_stage1_policy_to_onnx,
    load_stage1_policy,
)


pytestmark = pytest.mark.skipif(ort is None, reason="onnxruntime not installed")


def test_locked_stage1_policy_spec_matches_bridge_contract():
    spec = Stage1PolicySpec.default()

    assert spec.checkpoint_path == DEFAULT_CHECKPOINT_PATH
    assert spec.output_path == DEFAULT_OUTPUT_PATH
    assert spec.ue_import_path == DEFAULT_UE_IMPORT_PATH
    assert spec.input_names == ("self_obs", "mimic_target_poses", "terrain")
    assert spec.input_dims == {
        "self_obs": 358,
        "mimic_target_poses": 6495,
        "terrain": 256,
    }
    assert spec.output_name == "actions"
    assert spec.output_dim == 69
    assert spec.opset_fallbacks == (17, 16, 15)


def test_load_stage1_policy_returns_real_pretrained_actor():
    loaded = load_stage1_policy()

    assert loaded.spec.input_dims["self_obs"] == 358
    assert loaded.spec.input_dims["mimic_target_poses"] == 6495
    assert loaded.spec.input_dims["terrain"] == 256
    assert loaded.spec.output_dim == 69

    inputs = (
        loaded.spec.make_dummy_tensor("self_obs"),
        loaded.spec.make_dummy_tensor("mimic_target_poses"),
        loaded.spec.make_dummy_tensor("terrain"),
    )
    outputs = loaded.wrapper(
        inputs[0],
        inputs[1],
        inputs[2],
    )
    reference_outputs = loaded.model.act(
        {
            "self_obs": inputs[0],
            "mimic_target_poses": inputs[1],
            "terrain": inputs[2],
        }
    )
    assert np.allclose(
        outputs.detach().cpu().numpy(),
        reference_outputs.detach().cpu().numpy(),
        atol=1.0e-6,
    )
    assert tuple(outputs.shape) == (1, 69)
    assert outputs.dtype.is_floating_point
    assert np.isfinite(outputs.detach().cpu().numpy()).all()


def test_export_writes_named_stage1_inputs_and_output(tmp_path: Path):
    loaded = load_stage1_policy()
    output_path = tmp_path / "phc_policy.onnx"

    accepted_opset = export_stage1_policy_to_onnx(loaded, output_path)

    assert accepted_opset == 17
    assert output_path.exists()

    model = onnx.load(output_path)
    onnx.checker.check_model(model)

    input_names = [value.name for value in model.graph.input]
    output_names = [value.name for value in model.graph.output]

    assert input_names == list(loaded.spec.input_names)
    assert output_names == [loaded.spec.output_name]


def test_exported_model_matches_pytorch_actor_numerically(tmp_path: Path):
    loaded = load_stage1_policy()
    output_path = tmp_path / "phc_policy.onnx"
    export_stage1_policy_to_onnx(loaded, output_path)

    parity = compare_pytorch_and_onnx(loaded, output_path, seed=7)

    assert parity.output_name == "actions"
    assert parity.max_abs_diff < 1.0e-4
    assert parity.mean_abs_diff < 1.0e-5

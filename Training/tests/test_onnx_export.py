"""
Test suite for ONNX model export and validation.
Tests are written BEFORE the implementation (TDD).

Run: pytest Training/tests/test_onnx_export.py -v
"""
import pytest
import numpy as np
import os

try:
    import onnxruntime as ort
    HAS_ORT = True
except ImportError:
    HAS_ORT = False


# Path to the exported model (created by export_onnx.py workflow)
MODEL_PATH = os.path.join(
    os.path.dirname(__file__), "..", "output", "phc_policy.onnx"
)

pytestmark = pytest.mark.skipif(
    not HAS_ORT or not os.path.exists(MODEL_PATH),
    reason="onnxruntime not installed or model not yet exported"
)


class TestONNXModelLoads:
    """Verify the exported ONNX model loads correctly."""

    def test_model_loads(self):
        """ONNX file must load without error."""
        session = ort.InferenceSession(MODEL_PATH)
        assert session is not None

    def test_input_shape(self):
        """Model must accept the correct input tensor shape."""
        session = ort.InferenceSession(MODEL_PATH)
        inputs = session.get_inputs()
        assert len(inputs) >= 1, "Model must have at least one input"
        input_shape = inputs[0].shape
        # PHC observation is typically ~131-400 floats depending on version
        # The exact shape will be determined after setting up ProtoMotions
        assert len(input_shape) == 2, f"Expected 2D input [batch, obs_dim], got shape {input_shape}"
        assert input_shape[0] == 1 or input_shape[0] is None, "Batch dim should be 1 or dynamic"

    def test_output_shape(self):
        """Model must output the correct shape."""
        session = ort.InferenceSession(MODEL_PATH)
        outputs = session.get_outputs()
        assert len(outputs) >= 1, "Model must have at least one output"
        output_shape = outputs[0].shape
        assert len(output_shape) == 2, f"Expected 2D output [batch, action_dim], got shape {output_shape}"


class TestONNXInference:
    """Verify inference produces valid results."""

    def _run_inference(self, obs):
        """Helper to run a single inference."""
        session = ort.InferenceSession(MODEL_PATH)
        input_name = session.get_inputs()[0].name
        input_shape = session.get_inputs()[0].shape
        obs_dim = input_shape[1]
        input_data = obs if obs is not None else np.zeros((1, obs_dim), dtype=np.float32)
        result = session.run(None, {input_name: input_data})
        return result[0]

    def test_output_range(self):
        """Output values must be in a plausible range (no NaN/Inf)."""
        output = self._run_inference(None)
        assert not np.any(np.isnan(output)), "Output contains NaN"
        assert not np.any(np.isinf(output)), "Output contains Inf"
        # Joint targets should be in a reasonable range
        assert np.all(np.abs(output) < 100), f"Output values unreasonably large: max={np.max(np.abs(output))}"

    def test_deterministic(self):
        """Same input must produce the same output (no randomness at inference)."""
        rng = np.random.RandomState(42)
        session = ort.InferenceSession(MODEL_PATH)
        input_name = session.get_inputs()[0].name
        obs_dim = session.get_inputs()[0].shape[1]
        obs = rng.randn(1, obs_dim).astype(np.float32)

        result1 = session.run(None, {input_name: obs})[0]
        result2 = session.run(None, {input_name: obs})[0]

        np.testing.assert_array_equal(result1, result2,
                                       err_msg="Inference is not deterministic")

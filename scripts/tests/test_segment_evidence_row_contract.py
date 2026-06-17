import unittest

def validate_segment_evidence_row(row: dict) -> list[str]:
    errors = []
    
    # Check required fields
    required_fields = ["segment_name", "state", "metrics", "missing_required_fields", "source_provenance"]
    for field in required_fields:
        if field not in row:
            errors.append(f"Missing required field: {field}")
            
    # Check state vocabulary
    allowed_states = {"NotReached", "ReachedButInactive", "Active"}
    if row.get("state") not in allowed_states:
        errors.append(f"Invalid state: {row.get('state')}. Must be one of {allowed_states}")
        
    # Check metrics is an object
    if "metrics" in row and not isinstance(row["metrics"], dict):
        errors.append("metrics must be an object (dict)")
        
    # Check missing_required_fields is an explicitly reported list
    if "missing_required_fields" in row and not isinstance(row["missing_required_fields"], list):
        errors.append("missing_required_fields must be a list")

    # Check source_provenance is an explicitly reported list
    if "source_provenance" in row and not isinstance(row["source_provenance"], list):
        errors.append("source_provenance must be a list")
        
    return errors

class TestSegmentEvidenceRowContract(unittest.TestCase):
    def test_real_emitted_summary_segment_shape(self):
        emitted_row = {
            "segment_name": "PoseSearch",
            "state": "Active",
            "metrics": {
                "sample_count": 90,
                "confidence": 1.0,
                "score": 90.0,
                "selected_source_identity": "MM_Idle",
                "selected_source_time": 0.0,
                "consecutive_invalid_sample_count": 0,
                "inference_attempt_count": 0,
                "inference_failure_count": 0,
                "inference_latency_ms_max": 0.0,
                "model_loaded": False,
                "runtime_name": "",
                "input_buffers_finite": False,
            },
            "missing_required_fields": [],
            "diagnostic_notes": [],
            "source_provenance": ["pose_search", "EB-01"],
        }

        self.assertEqual(validate_segment_evidence_row(emitted_row), [])

    def test_valid_row(self):
        valid_row = {
            "segment_name": "PoseSearch",
            "state": "Active",
            "metrics": {"selected_source_identity": "run_fwd"},
            "missing_required_fields": [],
            "source_provenance": ["summary_artifact"],
            "diagnostic_notes": "All good"
        }
        self.assertEqual(validate_segment_evidence_row(valid_row), [])
        
    def test_invalid_state(self):
        row = {
            "segment_name": "PhcPolicy",
            "state": "Running",
            "metrics": {},
            "missing_required_fields": [],
            "source_provenance": ["terminal_artifact"]
        }
        errors = validate_segment_evidence_row(row)
        self.assertTrue(any("Invalid state" in err for err in errors))
        
    def test_missing_fields(self):
        row = {
            "segment_name": "PhysicsControl",
            "state": "NotReached"
        }
        errors = validate_segment_evidence_row(row)
        self.assertIn("Missing required field: metrics", errors)
        self.assertIn("Missing required field: missing_required_fields", errors)
        self.assertIn("Missing required field: source_provenance", errors)
        
if __name__ == '__main__':
    unittest.main()

import unittest

def validate_segment_evidence_row(row: dict) -> list[str]:
    errors = []
    
    # Check required fields
    required_fields = ["segment_name", "state", "required_metrics", "missing_required_fields", "provenance"]
    for field in required_fields:
        if field not in row:
            errors.append(f"Missing required field: {field}")
            
    # Check state vocabulary
    allowed_states = {"NotReached", "ReachedButInactive", "Active"}
    if row.get("state") not in allowed_states:
        errors.append(f"Invalid state: {row.get('state')}. Must be one of {allowed_states}")
        
    # Check required_metrics is an object
    if "required_metrics" in row and not isinstance(row["required_metrics"], dict):
        errors.append("required_metrics must be an object (dict)")
        
    # Check missing_required_fields is an explicitly reported list
    if "missing_required_fields" in row and not isinstance(row["missing_required_fields"], list):
        errors.append("missing_required_fields must be a list")
        
    return errors

class TestSegmentEvidenceRowContract(unittest.TestCase):
    def test_valid_row(self):
        valid_row = {
            "segment_name": "PoseSearch",
            "state": "Active",
            "required_metrics": {"anim_name": "run_fwd"},
            "missing_required_fields": [],
            "provenance": "summary_artifact",
            "diagnostic_notes": "All good"
        }
        self.assertEqual(validate_segment_evidence_row(valid_row), [])
        
    def test_invalid_state(self):
        row = {
            "segment_name": "PhcPolicy",
            "state": "Running",
            "required_metrics": {},
            "missing_required_fields": [],
            "provenance": "terminal_artifact"
        }
        errors = validate_segment_evidence_row(row)
        self.assertTrue(any("Invalid state" in err for err in errors))
        
    def test_missing_fields(self):
        row = {
            "segment_name": "PhysicsControl",
            "state": "NotReached"
        }
        errors = validate_segment_evidence_row(row)
        self.assertIn("Missing required field: required_metrics", errors)
        self.assertIn("Missing required field: missing_required_fields", errors)
        self.assertIn("Missing required field: provenance", errors)
        
if __name__ == '__main__':
    unittest.main()

"""
Test suite for SMPL ↔ UE5 skeleton retargeting.
Tests are written BEFORE the implementation (TDD).

Run: pytest Training/tests/test_retarget.py -v
"""
import pytest
import numpy as np

# This import will fail until we implement the module
# That's intentional — TDD: write tests first, then make them pass
try:
    from physanim.retarget import (
        SMPLToUE5Mapper,
        SMPL_JOINT_NAMES,
        UE5_BONE_NAMES,
        smpl_to_ue5_rotation,
        ue5_to_smpl_rotation,
    )
    HAS_RETARGET = True
except ImportError:
    HAS_RETARGET = False


pytestmark = pytest.mark.skipif(
    not HAS_RETARGET,
    reason="physanim.retarget not yet implemented"
)


class TestSMPLJointDefinitions:
    """Verify SMPL skeleton constants are correct."""

    def test_smpl_joint_count(self):
        """SMPL skeleton must have exactly 24 joints."""
        assert len(SMPL_JOINT_NAMES) == 24

    def test_smpl_joint_names_known(self):
        """All expected SMPL joint names must be present."""
        expected = [
            "Pelvis", "L_Hip", "R_Hip", "Spine1", "L_Knee", "R_Knee",
            "Spine2", "L_Ankle", "R_Ankle", "Spine3", "L_Foot", "R_Foot",
            "Neck", "L_Collar", "R_Collar", "Head", "L_Shoulder", "R_Shoulder",
            "L_Elbow", "R_Elbow", "L_Wrist", "R_Wrist", "L_Hand", "R_Hand"
        ]
        for name in expected:
            assert name in SMPL_JOINT_NAMES, f"Missing SMPL joint: {name}"


class TestUE5BoneMapping:
    """Verify UE5 bone mapping covers all SMPL joints."""

    def test_ue5_mapping_covers_all_smpl_joints(self):
        """Every SMPL joint (except hand tips) must map to a UE5 bone."""
        mapper = SMPLToUE5Mapper()
        unmapped_allowed = {"L_Hand", "R_Hand"}  # Finger tips, no UE5 equivalent
        for joint in SMPL_JOINT_NAMES:
            if joint not in unmapped_allowed:
                assert mapper.has_mapping(joint), f"SMPL joint '{joint}' has no UE5 bone mapping"

    def test_ue5_bone_names_valid(self):
        """All mapped UE5 bone names must be valid UE5 mannequin bones."""
        mapper = SMPLToUE5Mapper()
        valid_ue5_bones = {
            "pelvis", "thigh_l", "thigh_r", "calf_l", "calf_r",
            "foot_l", "foot_r", "ball_l", "ball_r",
            "spine_01", "spine_02", "spine_03",
            "neck_01", "head",
            "clavicle_l", "clavicle_r",
            "upperarm_l", "upperarm_r",
            "lowerarm_l", "lowerarm_r",
            "hand_l", "hand_r",
        }
        for ue5_bone in mapper.get_all_ue5_bones():
            assert ue5_bone in valid_ue5_bones, f"Invalid UE5 bone name: {ue5_bone}"


class TestCoordinateConversion:
    """Verify Y-up (SMPL) ↔ Z-up (UE5) coordinate system conversion."""

    def test_identity_pose(self):
        """T-pose (identity) in SMPL must produce T-pose in UE5."""
        smpl_pose = np.zeros(72)  # 24 joints × 3 axis-angle = all zeros = T-pose
        mapper = SMPLToUE5Mapper()
        ue5_rotations = mapper.smpl_pose_to_ue5(smpl_pose)
        # Each UE5 rotation should be near identity (quaternion ~[1,0,0,0] or [0,0,0,1])
        for bone_name, quat in ue5_rotations.items():
            # Check it's close to identity (accounting for reference pose differences)
            assert np.allclose(np.abs(quat), [1, 0, 0, 0], atol=0.1) or \
                   np.allclose(np.abs(quat), [0, 0, 0, 1], atol=0.1), \
                   f"Bone '{bone_name}' not identity in T-pose: {quat}"

    def test_coordinate_system_yup_to_zup(self):
        """A pure Y-up vector in SMPL must become Z-up in UE5."""
        # SMPL Y-up = (0, 1, 0) should map to UE5 Z-up = (0, 0, 1)
        smpl_up = np.array([0.0, 1.0, 0.0])
        ue5_up = smpl_to_ue5_rotation(smpl_up)
        expected = np.array([0.0, 0.0, 1.0])
        np.testing.assert_allclose(ue5_up, expected, atol=1e-6,
                                   err_msg="Y-up → Z-up conversion failed")

    def test_roundtrip(self):
        """SMPL → UE5 → SMPL must reproduce the original pose."""
        rng = np.random.RandomState(42)
        # Random but small rotations (within plausible joint range)
        smpl_pose = rng.randn(72) * 0.3

        mapper = SMPLToUE5Mapper()
        ue5_rotations = mapper.smpl_pose_to_ue5(smpl_pose)
        smpl_roundtrip = mapper.ue5_to_smpl_pose(ue5_rotations)

        np.testing.assert_allclose(smpl_roundtrip, smpl_pose, atol=1e-5,
                                   err_msg="Roundtrip conversion lost precision")


class TestSingleJointRotation:
    """Verify individual joint rotations map correctly."""

    def test_elbow_flexion(self):
        """90° elbow flexion in SMPL must produce correct UE5 lowerarm rotation."""
        smpl_pose = np.zeros(72)
        # L_Elbow is joint index 18, axis-angle for 90° around X axis
        smpl_pose[18 * 3: 18 * 3 + 3] = [np.pi / 2, 0, 0]

        mapper = SMPLToUE5Mapper()
        ue5_rotations = mapper.smpl_pose_to_ue5(smpl_pose)

        # lowerarm_l should have a ~90° rotation
        quat = ue5_rotations["lowerarm_l"]
        # Quaternion magnitude of axis components should be ~sin(45°) ≈ 0.707
        axis_magnitude = np.linalg.norm(quat[1:] if quat[0] > 0.5 else quat[:3])
        assert axis_magnitude > 0.5, f"Elbow rotation too small: {quat}"

    def test_hip_abduction(self):
        """30° hip abduction in SMPL must produce rotation in UE5 thigh."""
        smpl_pose = np.zeros(72)
        # R_Hip is joint index 2, 30° around Z axis
        smpl_pose[2 * 3: 2 * 3 + 3] = [0, 0, np.pi / 6]

        mapper = SMPLToUE5Mapper()
        ue5_rotations = mapper.smpl_pose_to_ue5(smpl_pose)

        quat = ue5_rotations["thigh_r"]
        # Should not be identity
        identity_dist = min(
            np.linalg.norm(quat - np.array([1, 0, 0, 0])),
            np.linalg.norm(quat - np.array([0, 0, 0, 1]))
        )
        assert identity_dist > 0.1, f"Hip rotation not detected: {quat}"

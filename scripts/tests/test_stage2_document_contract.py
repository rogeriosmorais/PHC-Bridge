from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).parents[2]


def test_stage2a_contract_uses_scripted_shell_authority_not_character_movement() -> None:
    text = (ROOT / "docs" / "contracts" / "S2A-KINEMATIC-ROOT-LOCOMOTION.md").read_text(encoding="utf-8")

    assert "does **not** claim self-propelled physical locomotion" in text
    assert "`CharacterMovementComponent` must be inactive" in text
    assert "Stage2A_KinematicShell" in text
    assert "physical-root progress projected onto the shell route" in text
    assert "as defined by the Unreal Engine `CharacterMovementComponent`" not in text


def test_frame_glossary_names_all_critical_boundaries() -> None:
    text = (ROOT / "docs" / "contracts" / "STAGE2-FRAME-GLOSSARY.md").read_text(encoding="utf-8")

    for required in (
        "UE world",
        "Actor",
        "Skeletal mesh",
        "Animation data",
        "Canonical SMPL / Proto world",
        "Policy heading-local",
        "Manny parent-relative target",
        "Kinematic shell",
        "Physical root",
    ):
        assert required in text


def test_architecture_keeps_product_verdict_external() -> None:
    text = (ROOT / "docs" / "architecture" / "STAGE2A-RUNTIME-ARCHITECTURE.md").read_text(encoding="utf-8")

    assert "Versioned protocol evaluator" in text
    assert "Runtime-generated PASS" in text
    assert "Pose Search database query" in text
    assert "Parent-relative Physics Control targets" in text
    assert "No evidence flag, debug trace, render setting, or evaluator threshold may change runtime behavior" in text

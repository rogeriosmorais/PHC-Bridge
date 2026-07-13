from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


def test_runner_uses_locked_v2_and_stops_at_first_non_pass() -> None:
    source = (REPO_ROOT / "scripts" / "run_standing_plant_ladder.ps1").read_text(
        encoding="utf-8"
    )

    assert "standing-plant-ladder.v2.json" in source
    assert "$Protocol.ordered_layers" in source
    assert "PhysAnim.Development.StandingPlant.$Layer" in source
    assert "--manifest $ManifestPath" in source
    assert 'if ($Evaluation.status -ne "PASS")' in source
    assert "break" in source
    assert "physanim-development-ladder-summary/v1" in source
    assert "standing-plant-ladder.v1.json" not in source
    assert "evaluate_causal_standing.py" not in source


def test_runner_keeps_raw_runs_local_and_append_only() -> None:
    source = (REPO_ROOT / "scripts" / "run_standing_plant_ladder.ps1").read_text(
        encoding="utf-8"
    )

    assert "F:\\NewEngine\\" not in source
    assert "F:\\GlobalMCP" not in source
    assert "New-Item -ItemType Directory -Path $SessionRoot -ErrorAction Stop" in source
    assert "-TestMode RenderOffscreen" in source
    assert "-SourceCommit $SourceCommit" in source
    assert "-ModelOnnxSha256 $ModelHash" in source

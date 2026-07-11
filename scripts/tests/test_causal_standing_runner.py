from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


def test_runner_is_local_append_only_and_uses_fixed_variants() -> None:
    source = (REPO_ROOT / "scripts" / "run_causal_standing.ps1").read_text(encoding="utf-8")

    assert "F:\\NewEngine\\" not in source
    assert "F:\\GlobalMCP" not in source
    assert "New-Item -ItemType Directory -Path $SessionRoot -ErrorAction Stop" in source
    assert "$Protocol.variants" in source
    assert "$Protocol.repetitions.$Variant" in source
    assert "PhysAnim.Product.CausalStanding.$Variant" in source
    assert "-TestMode RenderOffscreen" in source
    assert "--bundle $ManifestPaths" in source


def test_build_script_supports_one_build_many_renderer_runs() -> None:
    source = (REPO_ROOT / "scripts" / "build.ps1").read_text(encoding="utf-8")

    assert '[ValidateSet("NullRHI", "RenderOffscreen")]' in source
    assert "[switch]$SkipBuild" in source
    assert 'if (-not $SkipBuild)' in source
    assert '$EditorArguments += "-RenderOffscreen"' in source
    assert '$EditorArguments += "-ReportExportPath=$ReportExportPath"' in source
    assert '$EditorArguments += "-PhysAnimProductRunRoot=$ProductRunRoot"' in source

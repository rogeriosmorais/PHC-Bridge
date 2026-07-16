from __future__ import annotations

import json
import re
import subprocess
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SAFETY_MODULE = REPO_ROOT / "scripts" / "UnrealProcessSafety.psm1"
BUILD_SCRIPT = REPO_ROOT / "scripts" / "build.ps1"
ORCHESTRATION_SCRIPT = REPO_ROOT / "scripts" / "run_ue_automation_episode.ps1"
SCRIPT_SUFFIXES = {".ps1", ".psm1", ".bat", ".cmd", ".py", ".js", ".ts", ".sh"}
EXCLUDED_PARTS = {
    ".git",
    ".worktrees",
    ".codex-tmp",
    ".cache",
    ".venv",
    "venv",
    "Binaries",
    "Intermediate",
    "Saved",
    "node_modules",
    "_build",
    "_tmp",
    "__pycache__",
}


def _run_powershell(script: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            "powershell.exe",
            "-NoLogo",
            "-NoProfile",
            "-NonInteractive",
            "-ExecutionPolicy",
            "Bypass",
            "-Command",
            script,
        ],
        cwd=REPO_ROOT,
        text=True,
        capture_output=True,
        check=False,
    )


def _iter_project_scripts() -> list[Path]:
    result: list[Path] = []
    for path in REPO_ROOT.rglob("*"):
        if not path.is_file() or path.suffix.casefold() not in SCRIPT_SUFFIXES:
            continue
        relative = path.relative_to(REPO_ROOT)
        if any(part in EXCLUDED_PARTS for part in relative.parts):
            continue
        if len(relative.parts) >= 2 and relative.parts[0:2] == ("scripts", "tests"):
            continue
        result.append(path)
    return sorted(result)


def test_no_project_script_contains_an_unscoped_process_kill() -> None:
    violations: list[str] = []
    forbidden = re.compile(
        r"(?i)(?:\btaskkill(?:\.exe)?\b|\bpkill\b|\bkillall\b|"
        r"\bTerminateProcess\b|\.Kill\s*\(|\bStop-Process\b)"
    )
    for path in _iter_project_scripts():
        if path == SAFETY_MODULE:
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        if forbidden.search(text):
            violations.append(str(path.relative_to(REPO_ROOT)))

    assert violations == [], (
        "Process termination is only allowed inside scripts/UnrealProcessSafety.psm1; "
        f"found unscoped termination in: {violations}"
    )


def test_build_script_uses_version_scoped_opt_in_cleanup() -> None:
    text = BUILD_SCRIPT.read_text(encoding="utf-8")
    assert "UnrealProcessSafety.psm1" in text
    assert "CloseEngineProcesses" in text
    assert "Assert-UnrealEngineVersion" in text
    assert "Get-EngineOwnedUnrealProcesses" in text
    assert "Stop-EngineOwnedUnrealProcesses" in text
    assert "Stop-Process" not in text
    assert "taskkill" not in text.casefold()


def test_orchestration_uses_version_scoped_process_checks_and_timeout_cleanup() -> None:
    text = ORCHESTRATION_SCRIPT.read_text(encoding="utf-8")
    assert "Assert-UnrealEngineVersion" in text
    assert "Get-EngineOwnedUnrealProcesses" in text
    assert "Stop-ValidatedUnrealProcessTree" in text
    assert "[switch]$PolicyInputProvenanceTrace" in text
    assert "PolicyInputProvenanceTrace = $PolicyInputProvenanceTraceLiteral" in text
    assert "policy_input_provenance_trace = [bool]$PolicyInputProvenanceTrace" in text
    assert "ProductProtocolPath = '$EscapedProtocolPath'" in text
    assert "taskkill" not in text.casefold()
    assert "Stop-Process" not in text


def test_orchestration_refreshes_completed_child_before_reading_exit_code() -> None:
    text = ORCHESTRATION_SCRIPT.read_text(encoding="utf-8")
    wait_index = text.index("$Process.WaitForExit()")
    refresh_index = text.index("$Process.Refresh()", wait_index)
    exit_code_index = text.index("$ExitCode = $Process.ExitCode", refresh_index)
    assert wait_index < refresh_index < exit_code_index


def test_safety_module_refuses_ue58_and_only_stops_ue57_owned_paths(tmp_path: Path) -> None:
    engine57 = tmp_path / "UE_5.7" / "Engine"
    engine58 = tmp_path / "UE_5.8" / "Engine"
    for engine, minor in ((engine57, 7), (engine58, 8)):
        (engine / "Build").mkdir(parents=True)
        (engine / "Build" / "Build.version").write_text(
            json.dumps({"MajorVersion": 5, "MinorVersion": minor, "PatchVersion": 0}),
            encoding="utf-8",
        )

    ue57_editor = engine57 / "Binaries" / "Win64" / "UnrealEditor.exe"
    ue58_editor = engine58 / "Binaries" / "Win64" / "UnrealEditor.exe"
    unrelated = tmp_path / "Tools" / "UnrealEditor.exe"
    module = str(SAFETY_MODULE).replace("'", "''")
    engine57_ps = str(engine57).replace("'", "''")
    engine58_ps = str(engine58).replace("'", "''")
    ue57_ps = str(ue57_editor).replace("'", "''")
    ue58_ps = str(ue58_editor).replace("'", "''")
    unrelated_ps = str(unrelated).replace("'", "''")

    script = f"""
$ErrorActionPreference = 'Stop'
Import-Module '{module}' -Force
$Processes = @(
    [pscustomobject]@{{ Id = 57; ProcessName = 'UnrealEditor'; Path = '{ue57_ps}' }},
    [pscustomobject]@{{ Id = 58; ProcessName = 'UnrealEditor'; Path = '{ue58_ps}' }},
    [pscustomobject]@{{ Id = 99; ProcessName = 'UnrealEditor'; Path = '{unrelated_ps}' }},
    [pscustomobject]@{{ Id = 100; ProcessName = 'UnrealEditor'; Path = $null }}
)
$Stopped = [System.Collections.Generic.List[int]]::new()
$Provider = {{ param($Names) return $Processes }}
$Stopper = {{ param($Process) $Stopped.Add([int]$Process.Id) | Out-Null }}
$Owned = @(Get-EngineOwnedUnrealProcesses -EngineRoot '{engine57_ps}' -ProcessProvider $Provider)
$null = @(Stop-EngineOwnedUnrealProcesses -EngineRoot '{engine57_ps}' -ExpectedMajorVersion 5 -ExpectedMinorVersion 7 -ProcessProvider $Provider -StopProcessAction $Stopper)
$Rejected58 = $false
try {{
    $null = @(Stop-EngineOwnedUnrealProcesses -EngineRoot '{engine58_ps}' -ExpectedMajorVersion 5 -ExpectedMinorVersion 7 -ProcessProvider $Provider -StopProcessAction $Stopper)
}} catch {{
    $Rejected58 = $true
}}
[pscustomobject]@{{
    OwnedIds = @($Owned | ForEach-Object {{ [int]$_.Id }})
    StoppedIds = @($Stopped)
    Rejected58 = $Rejected58
}} | ConvertTo-Json -Compress
"""
    completed = _run_powershell(script)
    assert completed.returncode == 0, completed.stderr
    result = json.loads(completed.stdout.strip().splitlines()[-1])
    assert result["OwnedIds"] == [57]
    assert result["StoppedIds"] == [57]
    assert result["Rejected58"] is True


def test_timeout_tree_cleanup_refuses_any_ue58_descendant(tmp_path: Path) -> None:
    engine57 = tmp_path / "UE_5.7" / "Engine"
    engine58 = tmp_path / "UE_5.8" / "Engine"
    for engine, minor in ((engine57, 7), (engine58, 8)):
        (engine / "Build").mkdir(parents=True)
        (engine / "Build" / "Build.version").write_text(
            json.dumps({"MajorVersion": 5, "MinorVersion": minor, "PatchVersion": 0}),
            encoding="utf-8",
        )

    module = str(SAFETY_MODULE).replace("'", "''")
    engine57_ps = str(engine57).replace("'", "''")
    ue57_editor = str(
        engine57 / "Binaries" / "Win64" / "UnrealEditor-Cmd.exe"
    ).replace("'", "''")
    ue57_worker = str(
        engine57 / "Binaries" / "Win64" / "ShaderCompileWorker.exe"
    ).replace("'", "''")
    ue58_editor = str(
        engine58 / "Binaries" / "Win64" / "UnrealEditor.exe"
    ).replace("'", "''")

    script = f"""
$ErrorActionPreference = 'Stop'
Import-Module '{module}' -Force
$SafeTree = @(
    [pscustomobject]@{{ ProcessId = 10; ParentProcessId = 0; Name = 'powershell.exe'; ExecutablePath = 'C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe' }},
    [pscustomobject]@{{ ProcessId = 11; ParentProcessId = 10; Name = 'cmd.exe'; ExecutablePath = 'C:\\Windows\\System32\\cmd.exe' }},
    [pscustomobject]@{{ ProcessId = 12; ParentProcessId = 11; Name = 'UnrealEditor-Cmd.exe'; ExecutablePath = '{ue57_editor}' }},
    [pscustomobject]@{{ ProcessId = 13; ParentProcessId = 12; Name = 'ShaderCompileWorker.exe'; ExecutablePath = '{ue57_worker}' }}
)
$UnsafeTree = @(
    [pscustomobject]@{{ ProcessId = 20; ParentProcessId = 0; Name = 'powershell.exe'; ExecutablePath = 'C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe' }},
    [pscustomobject]@{{ ProcessId = 21; ParentProcessId = 20; Name = 'UnrealEditor.exe'; ExecutablePath = '{ue58_editor}' }}
)
$SafeStopped = [System.Collections.Generic.List[int]]::new()
$UnsafeStopped = [System.Collections.Generic.List[int]]::new()
$SafeProvider = {{ return $SafeTree }}
$UnsafeProvider = {{ return $UnsafeTree }}
$SafeStopper = {{ param($ProcessRecord) $SafeStopped.Add([int]$ProcessRecord.ProcessId) | Out-Null }}
$UnsafeStopper = {{ param($ProcessRecord) $UnsafeStopped.Add([int]$ProcessRecord.ProcessId) | Out-Null }}
$null = @(Stop-ValidatedUnrealProcessTree -RootProcessId 10 -EngineRoot '{engine57_ps}' -ProcessSnapshotProvider $SafeProvider -StopProcessAction $SafeStopper)
$RejectedUnsafe = $false
try {{
    $null = @(Stop-ValidatedUnrealProcessTree -RootProcessId 20 -EngineRoot '{engine57_ps}' -ProcessSnapshotProvider $UnsafeProvider -StopProcessAction $UnsafeStopper)
}} catch {{
    $RejectedUnsafe = $true
}}
[pscustomobject]@{{
    SafeStopped = @($SafeStopped)
    UnsafeStopped = @($UnsafeStopped)
    RejectedUnsafe = $RejectedUnsafe
}} | ConvertTo-Json -Compress
"""
    completed = _run_powershell(script)
    assert completed.returncode == 0, completed.stderr
    result = json.loads(completed.stdout.strip().splitlines()[-1])
    assert set(result["SafeStopped"]) == {10, 11, 12, 13}
    assert result["UnsafeStopped"] == []
    assert result["RejectedUnsafe"] is True


def test_build_script_accepts_ue57_root_and_rejects_ue58_root(tmp_path: Path) -> None:
    engine57 = tmp_path / "UE_5.7" / "Engine"
    engine58 = tmp_path / "UE_5.8" / "Engine"
    for engine, minor in ((engine57, 7), (engine58, 8)):
        (engine / "Build").mkdir(parents=True)
        (engine / "Build" / "Build.version").write_text(
            json.dumps({"MajorVersion": 5, "MinorVersion": minor, "PatchVersion": 0}),
            encoding="utf-8",
        )

    def run_build(engine: Path) -> subprocess.CompletedProcess[str]:
        engine_ps = str(engine).replace("'", "''")
        command = (
            f"$env:UE5_PATH = '{engine_ps}'; "
            "& '.\\scripts\\build.ps1' -SkipBuild"
        )
        return _run_powershell(command)

    accepted = run_build(engine57)
    assert accepted.returncode == 0, accepted.stderr

    rejected = run_build(engine58)
    assert rejected.returncode != 0
    combined = f"{rejected.stdout}\n{rejected.stderr}"
    assert "expected Unreal Engine 5.7" in combined

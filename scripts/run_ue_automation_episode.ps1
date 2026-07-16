[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$TestName,

    [Parameter(Mandatory = $true)]
    [string]$ProtocolPath,

    [Parameter(Mandatory = $true)]
    [string]$Variant,

    [int]$Repetition = 1,
    [string]$OutputRoot,
    [string]$RunId,
    [string]$ModelPath,

    [ValidateSet("NullRHI", "RenderOffscreen")]
    [string]$TestMode = "RenderOffscreen",

    [ValidateRange(10, 7200)]
    [int]$TimeoutSeconds = 600,

    [string[]]$RequiredArtifactFields = @(
        "physics_samples",
        "policy_samples",
        "scenario_summary",
        "policy_input_snapshot",
        "render_capture"
    ),

    [switch]$PolicyInputProvenanceTrace,
    [switch]$AllowDirty,
    [switch]$SkipCompile,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$BuildScript = Join-Path $PSScriptRoot "build.ps1"
$Validator = Join-Path $PSScriptRoot "validate_ue_automation_run.py"
$FingerprintTool = Join-Path $PSScriptRoot "environment_fingerprint.py"
$LocalPathsScript = Join-Path $PSScriptRoot "local.paths.ps1"
$UnrealProcessSafetyModule = Join-Path $PSScriptRoot "UnrealProcessSafety.psm1"

if (-not $env:UE5_PATH) {
    if (-not (Test-Path -LiteralPath $LocalPathsScript -PathType Leaf)) {
        Write-Error "BLOCKED: UE5_PATH is unset and local.paths.ps1 is missing: $LocalPathsScript"
        exit 3
    }
    . $LocalPathsScript
}
Import-Module $UnrealProcessSafetyModule -Force -ErrorAction Stop
$EngineVersion = Assert-UnrealEngineVersion `
    -EngineRoot $env:UE5_PATH `
    -ExpectedMajorVersion 5 `
    -ExpectedMinorVersion 7

function Resolve-RepoPath([string]$Value) {
    if ([System.IO.Path]::IsPathRooted($Value)) {
        return [System.IO.Path]::GetFullPath($Value)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $Value))
}

function Write-OrchestrationResult([string]$Path, [hashtable]$Value) {
    $Value | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $Path -Encoding utf8
}

function Get-Sha256([string]$Path) {
    $Stream = [System.IO.File]::OpenRead($Path)
    try {
        $Hasher = [System.Security.Cryptography.SHA256]::Create()
        try {
            return ([System.BitConverter]::ToString($Hasher.ComputeHash($Stream))).Replace("-", "").ToLowerInvariant()
        }
        finally {
            $Hasher.Dispose()
        }
    }
    finally {
        $Stream.Dispose()
    }
}

foreach ($RequiredTool in @($BuildScript, $Validator, $FingerprintTool, $UnrealProcessSafetyModule)) {
    if (-not (Test-Path -LiteralPath $RequiredTool -PathType Leaf)) {
        Write-Error "BLOCKED: required orchestration tool is missing: $RequiredTool"
        exit 3
    }
}

$ProtocolPath = Resolve-RepoPath $ProtocolPath
if (-not (Test-Path -LiteralPath $ProtocolPath -PathType Leaf)) {
    Write-Error "BLOCKED: protocol is missing: $ProtocolPath"
    exit 3
}
$Protocol = Get-Content -Raw -LiteralPath $ProtocolPath | ConvertFrom-Json
if ($Protocol.status -ne "LOCKED") {
    Write-Error "INVALID: protocol must declare status LOCKED: $ProtocolPath"
    exit 2
}

if (-not $ModelPath) {
    $ModelPath = Join-Path $RepoRoot "PhysAnimUE5\Content\NNEModels\phc_policy.onnx"
}
$ModelPath = Resolve-RepoPath $ModelPath
if (-not (Test-Path -LiteralPath $ModelPath -PathType Leaf)) {
    Write-Error "BLOCKED: model file is missing: $ModelPath"
    exit 3
}

$SourceCommit = (& git -C $RepoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $SourceCommit -notmatch '^[0-9a-f]{40}$') {
    Write-Error "BLOCKED: could not resolve source commit"
    exit 3
}
$GitStatus = @(& git -C $RepoRoot status --porcelain=v1 --untracked-files=normal)
$SourceTreeDirty = $GitStatus.Count -gt 0
if ($SourceTreeDirty -and -not $AllowDirty) {
    Write-Error "INVALID: authoritative automation requires a clean source tree. Use -AllowDirty only for development diagnostics."
    exit 2
}

if (-not $DryRun) {
    $RunningUnreal = @(
        Get-EngineOwnedUnrealProcesses -EngineRoot $EngineVersion.EngineRoot
    )
    if ($RunningUnreal.Count -gt 0) {
        $ProcessSummary = ($RunningUnreal | ForEach-Object {
            "$($_.ProcessName):$($_.Id) [$($_.Path)]"
        }) -join ", "
        Write-Error (
            "BLOCKED: a UE 5.7 process owned by this project's configured engine is already running; " +
            "refusing to start a duplicate episode: $ProcessSummary. " +
            "Unreal processes outside $($EngineVersion.EngineRoot), including UE 5.8, are ignored."
        )
        exit 3
    }
}

if (-not $OutputRoot) {
    $OutputRoot = Join-Path $RepoRoot "test-results\ue-automation-episodes"
}
$OutputRoot = Resolve-RepoPath $OutputRoot
$NormalizedRepo = [System.IO.Path]::GetFullPath($RepoRoot).TrimEnd('\') + '\'
$NormalizedOutput = [System.IO.Path]::GetFullPath($OutputRoot)
if (-not $NormalizedOutput.StartsWith($NormalizedRepo, [System.StringComparison]::OrdinalIgnoreCase)) {
    Write-Error "INVALID: output root must remain inside the repository: $OutputRoot"
    exit 2
}

if (-not $RunId) {
    $Timestamp = (Get-Date).ToUniversalTime().ToString("yyyyMMddTHHmmssZ")
    $RunId = "$Timestamp-$Variant-$Repetition-$($SourceCommit.Substring(0, 8))-$([guid]::NewGuid().ToString('N').Substring(0, 8))"
}
if ($RunId -notmatch '^[A-Za-z0-9._-]+$') {
    Write-Error "INVALID: RunId contains unsupported characters: $RunId"
    exit 2
}

$RunRoot = Join-Path $OutputRoot $RunId
$ReportRoot = Join-Path $RunRoot "automation-report"
$InvocationPath = Join-Path $RunRoot "invoke-build.ps1"
$StdoutPath = Join-Path $RunRoot "automation.stdout.log"
$StderrPath = Join-Path $RunRoot "automation.stderr.log"
$ValidationPath = Join-Path $RunRoot "fixture-validation.json"
$FingerprintPath = Join-Path $RunRoot "environment-fingerprint.json"
$OrchestrationPath = Join-Path $RunRoot "orchestration.json"
$ChildExitCodePath = Join-Path $RunRoot "child-exit-code.txt"
$ModelHash = Get-Sha256 $ModelPath

$Plan = [ordered]@{
    schema_version = "physanim-ue-automation-plan/v1"
    authority = if ($SourceTreeDirty) { "DEVELOPMENT_DIRTY" } else { "COMMITTED_SOURCE" }
    run_id = $RunId
    run_root = $RunRoot
    report_root = $ReportRoot
    test_name = $TestName
    protocol_path = $ProtocolPath
    protocol_id = $Protocol.protocol_id
    protocol_version = $Protocol.version
    variant = $Variant
    repetition = $Repetition
    source_commit = $SourceCommit
    source_tree_dirty = $SourceTreeDirty
    model_path = $ModelPath
    model_sha256 = $ModelHash
    test_mode = $TestMode
    timeout_seconds = $TimeoutSeconds
    compile_before_run = -not $SkipCompile
    policy_input_provenance_trace = [bool]$PolicyInputProvenanceTrace
    required_artifact_fields = $RequiredArtifactFields
}

if ($DryRun) {
    $Plan.dry_run = $true
    $Plan | ConvertTo-Json -Depth 8
    exit 0
}

New-Item -ItemType Directory -Path $ReportRoot -Force | Out-Null
Write-OrchestrationResult $OrchestrationPath @{
    schema_version = "physanim-ue-automation-orchestration/v1"
    state = "PLANNED"
    plan = $Plan
}

$EscapedBuildScript = $BuildScript.Replace("'", "''")
$EscapedTestName = $TestName.Replace("'", "''")
$EscapedTestMode = $TestMode.Replace("'", "''")
$EscapedReportRoot = $ReportRoot.Replace("'", "''")
$EscapedRunRoot = $RunRoot.Replace("'", "''")
$EscapedRunId = $RunId.Replace("'", "''")
$EscapedVariant = $Variant.Replace("'", "''")
$EscapedProtocolPath = $ProtocolPath.Replace("'", "''")
$EscapedSourceCommit = $SourceCommit.Replace("'", "''")
$EscapedModelHash = $ModelHash.Replace("'", "''")
$EscapedChildExitCodePath = $ChildExitCodePath.Replace("'", "''")
$SkipBuildLiteral = if ($SkipCompile) { '$true' } else { '$false' }
$DirtyLiteral = if ($SourceTreeDirty) { '$true' } else { '$false' }
$PolicyInputProvenanceTraceLiteral = if ($PolicyInputProvenanceTrace) { '$true' } else { '$false' }

$Invocation = @"
`$ErrorActionPreference = 'Stop'
`$Parameters = @{
    SkipBuild = $SkipBuildLiteral
    Test = '$EscapedTestName'
    TestMode = '$EscapedTestMode'
    ReportExportPath = '$EscapedReportRoot'
    ProductRunRoot = '$EscapedRunRoot'
    ProductRunId = '$EscapedRunId'
    ProductVariant = '$EscapedVariant'
    ProductProtocolPath = '$EscapedProtocolPath'
    ProductRepetition = $Repetition
    SourceCommit = '$EscapedSourceCommit'
    ModelOnnxSha256 = '$EscapedModelHash'
    SourceTreeDirty = $DirtyLiteral
    PolicyInputProvenanceTrace = $PolicyInputProvenanceTraceLiteral
}
& '$EscapedBuildScript' @Parameters
`$BuildInvocationSucceeded = `$?
`$ChildExitCode = `$LASTEXITCODE
if (`$null -eq `$ChildExitCode) {
    `$ChildExitCode = if (`$BuildInvocationSucceeded) { 0 } else { 1 }
}
Set-Content -LiteralPath '$EscapedChildExitCodePath' -Value ([int]`$ChildExitCode) -Encoding ascii
exit ([int]`$ChildExitCode)
"@
$Invocation | Set-Content -LiteralPath $InvocationPath -Encoding utf8

Write-OrchestrationResult $OrchestrationPath @{
    schema_version = "physanim-ue-automation-orchestration/v1"
    state = "RUNNING"
    started_utc = (Get-Date).ToUniversalTime().ToString("o")
    plan = $Plan
}

$Process = Start-Process powershell.exe `
    -ArgumentList @("-NoLogo", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File", $InvocationPath) `
    -WorkingDirectory $RepoRoot `
    -RedirectStandardOutput $StdoutPath `
    -RedirectStandardError $StderrPath `
    -PassThru

$Deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
while (-not $Process.HasExited) {
    if ([DateTime]::UtcNow -ge $Deadline) {
        $StoppedProcesses = @(
            Stop-ValidatedUnrealProcessTree `
                -RootProcessId $Process.Id `
                -EngineRoot $EngineVersion.EngineRoot `
                -ExpectedMajorVersion 5 `
                -ExpectedMinorVersion 7
        )
        Write-OrchestrationResult $OrchestrationPath @{
            schema_version = "physanim-ue-automation-orchestration/v1"
            state = "TIMED_OUT"
            verdict = "BLOCKED"
            ended_utc = (Get-Date).ToUniversalTime().ToString("o")
            child_pid = $Process.Id
            stopped_process_ids = @($StoppedProcesses | ForEach-Object { [int]$_.ProcessId })
            plan = $Plan
            stdout = $StdoutPath
            stderr = $StderrPath
        }
        Write-Error "BLOCKED: automation exceeded $TimeoutSeconds seconds. Existing artifacts and logs were retained at $RunRoot"
        exit 3
    }
    Start-Sleep -Seconds 1
    $Process.Refresh()
}

# PowerShell can observe HasExited before redirected streams and managed
# process state are fully synchronized. The child writes its own integer exit
# marker; the Process.ExitCode property is only a fallback.
$Process.WaitForExit()
$Process.Refresh()
$ExitCode = $null
if (Test-Path -LiteralPath $ChildExitCodePath -PathType Leaf) {
    $RawChildExitCode = (Get-Content -Raw -LiteralPath $ChildExitCodePath).Trim()
    $ParsedChildExitCode = 0
    if ([int]::TryParse($RawChildExitCode, [ref]$ParsedChildExitCode)) {
        $ExitCode = $ParsedChildExitCode
    }
}
if ($null -eq $ExitCode) {
    try {
        $ExitCode = [int]$Process.ExitCode
    }
    catch {
        $ExitCode = $null
    }
}
if ($null -eq $ExitCode) {
    Write-OrchestrationResult $OrchestrationPath @{
        schema_version = "physanim-ue-automation-orchestration/v1"
        state = "CHILD_EXIT_UNKNOWN"
        verdict = "INVALID"
        ended_utc = (Get-Date).ToUniversalTime().ToString("o")
        plan = $Plan
        stdout = $StdoutPath
        stderr = $StderrPath
        child_exit_code_path = $ChildExitCodePath
    }
    Write-Error "INVALID: automation child completed without a readable exit code. Artifacts and logs were retained at $RunRoot"
    exit 2
}
if ($ExitCode -ne 0) {
    $StdoutTail = if (Test-Path $StdoutPath) { (Get-Content $StdoutPath -Tail 80) -join "`n" } else { "" }
    $Verdict = if ($StdoutTail -match 'COMPILATION FAILED|Result: Failed') { "BLOCKED" } else { "INVALID" }
    Write-OrchestrationResult $OrchestrationPath @{
        schema_version = "physanim-ue-automation-orchestration/v1"
        state = "CHILD_FAILED"
        verdict = $Verdict
        child_exit_code = $ExitCode
        ended_utc = (Get-Date).ToUniversalTime().ToString("o")
        plan = $Plan
        stdout = $StdoutPath
        stderr = $StderrPath
    }
    Write-Error "$Verdict`: automation process exited with code $ExitCode. Artifacts and logs were retained at $RunRoot"
    exit $(if ($Verdict -eq "BLOCKED") { 3 } else { 2 })
}

$ValidatorArguments = @(
    $Validator,
    "--run-root", $RunRoot,
    "--report-root", $ReportRoot,
    "--expected-test", $TestName,
    "--expected-source-commit", $SourceCommit,
    "--expected-protocol", $ProtocolPath,
    "--expected-variant", $Variant,
    "--expected-repetition", $Repetition,
    "--output", $ValidationPath
)
foreach ($Field in $RequiredArtifactFields) {
    $ValidatorArguments += @("--required-artifact-field", $Field)
}
& python @ValidatorArguments
$ValidationExitCode = $LASTEXITCODE
$Validation = if (Test-Path $ValidationPath) {
    Get-Content -Raw -LiteralPath $ValidationPath | ConvertFrom-Json
} else {
    $null
}
$FixtureVerdict = if ($Validation) { $Validation.verdict } else { "INVALID" }

$ReportIndexPath = Join-Path $ReportRoot "index.json"
$FingerprintArguments = @(
    $FingerprintTool,
    "--repo-root", $RepoRoot,
    "--protocol", $ProtocolPath,
    "--model", $ModelPath,
    "--source-commit", $SourceCommit,
    "--output", $FingerprintPath
)
if ($SourceTreeDirty) {
    $FingerprintArguments += "--source-tree-dirty"
}
if (Test-Path -LiteralPath $ReportIndexPath -PathType Leaf) {
    $FingerprintArguments += @("--automation-report", $ReportIndexPath)
}
& python @FingerprintArguments
$FingerprintExitCode = $LASTEXITCODE
if ($FingerprintExitCode -ne 0 -or -not (Test-Path -LiteralPath $FingerprintPath -PathType Leaf)) {
    $FixtureVerdict = "INVALID"
}

Write-OrchestrationResult $OrchestrationPath @{
    schema_version = "physanim-ue-automation-orchestration/v1"
    state = "COMPLETE"
    verdict = $FixtureVerdict
    note = "This verdict covers fixture identity, automation completion, and artifact completeness only. Behavioral product acceptance remains the versioned evaluator's responsibility."
    child_exit_code = $ExitCode
    validator_exit_code = $ValidationExitCode
    fingerprint_exit_code = $FingerprintExitCode
    environment_fingerprint = $FingerprintPath
    ended_utc = (Get-Date).ToUniversalTime().ToString("o")
    plan = $Plan
    validation = $Validation
    stdout = $StdoutPath
    stderr = $StderrPath
}

Write-Output "$FixtureVerdict`: UE automation episode retained at $RunRoot"
exit $(if ($FixtureVerdict -eq "PASS") { 0 } else { 2 })

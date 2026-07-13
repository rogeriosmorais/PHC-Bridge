[CmdletBinding()]
param(
    [string]$OutputRoot,
    [switch]$AllowDirty
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$ProtocolPath = Join-Path $RepoRoot "product-gates\standing-plant-ladder.v2.json"
$ModelPath = Join-Path $RepoRoot "PhysAnimUE5\Content\NNEModels\phc_policy.onnx"
$BuildScript = Join-Path $PSScriptRoot "build.ps1"
$Evaluator = Join-Path $PSScriptRoot "evaluate_standing_plant.py"

if (-not $OutputRoot) {
    $OutputRoot = Join-Path $RepoRoot "test-results\standing-plant-runs"
}
$OutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)
if (-not $OutputRoot.StartsWith([System.IO.Path]::GetFullPath($RepoRoot), [System.StringComparison]::OrdinalIgnoreCase)) {
    Write-Error "INVALID: standing-plant runs must stay inside $RepoRoot"
    exit 2
}

foreach ($RequiredPath in @($ProtocolPath, $ModelPath, $BuildScript, $Evaluator)) {
    if (-not (Test-Path -LiteralPath $RequiredPath -PathType Leaf)) {
        Write-Error "BLOCKED: required local file is missing: $RequiredPath"
        exit 3
    }
}

$Protocol = Get-Content -Raw -LiteralPath $ProtocolPath | ConvertFrom-Json
$ExpectedLayers = "ControlsOff,DampingOnly,FixedNeutralTarget,ZeroActions,RealOnnxPolicy"
if (
    $Protocol.status -ne "LOCKED" -or
    $Protocol.authority -ne "DEVELOPMENT_GATE_ONLY" -or
    $Protocol.protocol_id -ne "standing-plant-ladder" -or
    $Protocol.version -ne 2 -or
    ($Protocol.ordered_layers -join ",") -ne $ExpectedLayers
) {
    Write-Error "INVALID: standing-plant ladder v2 protocol is not locked and ordered"
    exit 2
}

$SourceCommit = (& git -C $RepoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $SourceCommit -notmatch '^[0-9a-f]{40}$') {
    Write-Error "BLOCKED: could not resolve the local source commit"
    exit 3
}
$GitStatus = (& git -C $RepoRoot status --porcelain=v1)
$SourceTreeDirty = @($GitStatus).Count -gt 0
if ($SourceTreeDirty -and -not $AllowDirty) {
    Write-Error "INVALID: immutable standing-plant evidence requires a clean committed source tree"
    exit 2
}

$ModelHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $ModelPath).Hash.ToLowerInvariant()
$ShortCommit = $SourceCommit.Substring(0, 8)
$Timestamp = (Get-Date).ToUniversalTime().ToString("yyyyMMddTHHmmssZ")
$SessionId = "$Timestamp-$ShortCommit-$([guid]::NewGuid().ToString('N').Substring(0, 8))"
$SessionRoot = Join-Path $OutputRoot $SessionId
New-Item -ItemType Directory -Path $SessionRoot -ErrorAction Stop | Out-Null

$Evaluations = @()
$FinalStatus = "PASS"
$StoppedAt = $null

Push-Location $RepoRoot
try {
    & $BuildScript
    if ($LASTEXITCODE -ne 0) {
        Write-Error "BLOCKED: UE build failed with exit code $LASTEXITCODE"
        exit 3
    }

    foreach ($Layer in $Protocol.ordered_layers) {
        $RunId = "$Layer-1-$([guid]::NewGuid().ToString('N').Substring(0, 8))"
        $RunRoot = Join-Path $SessionRoot $RunId
        $ReportRoot = Join-Path $RunRoot "automation-report"
        New-Item -ItemType Directory -Path $ReportRoot -Force -ErrorAction Stop | Out-Null

        & $BuildScript `
            -SkipBuild `
            -Test "PhysAnim.Development.StandingPlant.$Layer" `
            -TestMode RenderOffscreen `
            -ReportExportPath $ReportRoot `
            -ProductRunRoot $RunRoot `
            -ProductRunId $RunId `
            -ProductVariant $Layer `
            -ProductRepetition 1 `
            -SourceCommit $SourceCommit `
            -ModelOnnxSha256 $ModelHash `
            -SourceTreeDirty $SourceTreeDirty

        if ($LASTEXITCODE -ne 0) {
            $Evaluation = [pscustomobject]@{
                schema_version = "physanim-development-evaluation/v1"
                run_id = $RunId
                layer = $Layer
                repetition = 1
                status = "INVALID"
                fixture_authority = "DERIVED_ONLY"
                failed_criteria = @("ue_automation")
                error = "UE automation exited with code $LASTEXITCODE"
            }
            $Evaluations += $Evaluation
            $FinalStatus = "INVALID"
            $StoppedAt = $Layer
            break
        }

        $ManifestPath = Join-Path $RunRoot "manifest.json"
        if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
            $Evaluation = [pscustomobject]@{
                schema_version = "physanim-development-evaluation/v1"
                run_id = $RunId
                layer = $Layer
                repetition = 1
                status = "INVALID"
                fixture_authority = "DERIVED_ONLY"
                failed_criteria = @("missing_manifest")
                error = "UE test did not produce $ManifestPath"
            }
        }
        else {
            $EvaluationPath = Join-Path $RunRoot "evaluation.json"
            & python $Evaluator --manifest $ManifestPath --output $EvaluationPath
            if (-not (Test-Path -LiteralPath $EvaluationPath -PathType Leaf)) {
                $Evaluation = [pscustomobject]@{
                    schema_version = "physanim-development-evaluation/v1"
                    run_id = $RunId
                    layer = $Layer
                    repetition = 1
                    status = "INVALID"
                    fixture_authority = "DERIVED_ONLY"
                    failed_criteria = @("evaluator_execution")
                    error = "Standing-plant evaluator produced no result"
                }
            }
            else {
                $Evaluation = Get-Content -Raw -LiteralPath $EvaluationPath | ConvertFrom-Json
            }
        }

        $Evaluations += $Evaluation
        if ($Evaluation.status -ne "PASS") {
            $FinalStatus = [string]$Evaluation.status
            $StoppedAt = $Layer
            break
        }
    }

    $Summary = [pscustomobject]@{
        schema_version = "physanim-development-ladder-summary/v1"
        authority = "DEVELOPMENT_GATE_ONLY"
        protocol_path = $ProtocolPath
        source_commit = $SourceCommit
        source_tree_dirty = $SourceTreeDirty
        model_onnx_sha256 = $ModelHash
        session_root = $SessionRoot
        status = $FinalStatus
        stopped_at = $StoppedAt
        runs = $Evaluations
    }
    $SummaryPath = Join-Path $SessionRoot "ladder-summary.json"
    $Summary | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $SummaryPath -Encoding utf8
    Write-Output "Standing plant ladder $FinalStatus; summary: $SummaryPath"
    exit @{ PASS = 0; FAIL = 1; INVALID = 2; BLOCKED = 3 }[$FinalStatus]
}
finally {
    Pop-Location
}

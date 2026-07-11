[CmdletBinding()]
param(
    [ValidateSet("Development", "Milestone")]
    [string]$Mode = "Milestone",
    [string]$OutputRoot,
    [switch]$AllowDirty
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$ProtocolPath = Join-Path $RepoRoot "product-gates\causal-standing.v1.json"
$ModelPath = Join-Path $RepoRoot "PhysAnimUE5\Content\NNEModels\phc_policy.onnx"
$BuildScript = Join-Path $PSScriptRoot "build.ps1"
$Evaluator = Join-Path $PSScriptRoot "evaluate_causal_standing.py"

if (-not $OutputRoot) {
    $OutputRoot = Join-Path $RepoRoot "test-results\product-runs"
}
$OutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)
if (-not $OutputRoot.StartsWith([System.IO.Path]::GetFullPath($RepoRoot), [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Product runs must stay inside $RepoRoot"
}

foreach ($RequiredPath in @($ProtocolPath, $ModelPath, $BuildScript, $Evaluator)) {
    if (-not (Test-Path -LiteralPath $RequiredPath -PathType Leaf)) {
        Write-Error "BLOCKED: required local file is missing: $RequiredPath"
        exit 3
    }
}

$Protocol = Get-Content -Raw -LiteralPath $ProtocolPath | ConvertFrom-Json
if ($Protocol.status -ne "LOCKED" -or $Protocol.protocol_id -ne "causal-standing" -or $Protocol.version -ne 1) {
    Write-Error "INVALID: causal-standing v1 protocol is not locked"
    exit 2
}

$SourceCommit = (& git -C $RepoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $SourceCommit -notmatch '^[0-9a-f]{40}$') {
    Write-Error "BLOCKED: could not resolve the local source commit"
    exit 3
}
$GitStatus = (& git -C $RepoRoot status --porcelain=v1)
$SourceTreeDirty = @($GitStatus).Count -gt 0
if ($Mode -eq "Milestone" -and $SourceTreeDirty -and -not $AllowDirty) {
    Write-Error "INVALID: milestone runs require a clean committed source tree"
    exit 2
}

$ModelHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $ModelPath).Hash.ToLowerInvariant()
$ShortCommit = $SourceCommit.Substring(0, 8)
$Timestamp = (Get-Date).ToUniversalTime().ToString("yyyyMMddTHHmmssZ")
$SessionId = "$Timestamp-$ShortCommit-$([guid]::NewGuid().ToString('N').Substring(0, 8))"
$SessionRoot = Join-Path $OutputRoot $SessionId
New-Item -ItemType Directory -Path $SessionRoot -ErrorAction Stop | Out-Null

$RunSpecifications = @()
if ($Mode -eq "Milestone") {
    foreach ($Variant in $Protocol.variants) {
        $Count = [int]$Protocol.repetitions.$Variant
        for ($Repetition = 1; $Repetition -le $Count; $Repetition++) {
            $RunSpecifications += [pscustomobject]@{ Variant = $Variant; Repetition = $Repetition }
        }
    }
}
else {
    foreach ($Variant in $Protocol.variants) {
        $RunSpecifications += [pscustomobject]@{ Variant = $Variant; Repetition = 1 }
    }
}

Push-Location $RepoRoot
try {
    & $BuildScript
    if ($LASTEXITCODE -ne 0) {
        Write-Error "BLOCKED: UE build failed with exit code $LASTEXITCODE"
        exit 3
    }

    $ManifestPaths = @()
    foreach ($Specification in $RunSpecifications) {
        $Variant = $Specification.Variant
        $Repetition = $Specification.Repetition
        $RunId = "$Variant-$Repetition-$([guid]::NewGuid().ToString('N').Substring(0, 8))"
        $RunRoot = Join-Path $SessionRoot $RunId
        $ReportRoot = Join-Path $RunRoot "automation-report"
        New-Item -ItemType Directory -Path $ReportRoot -ErrorAction Stop | Out-Null

        $TestName = "PhysAnim.Product.CausalStanding.$Variant"
        & $BuildScript `
            -SkipBuild `
            -Test $TestName `
            -TestMode RenderOffscreen `
            -ReportExportPath $ReportRoot `
            -ProductRunRoot $RunRoot `
            -ProductRunId $RunId `
            -ProductVariant $Variant `
            -ProductRepetition $Repetition `
            -SourceCommit $SourceCommit `
            -ModelOnnxSha256 $ModelHash
        if ($LASTEXITCODE -ne 0) {
            Write-Error "INVALID: UE automation failed for $Variant repetition $Repetition"
            exit 2
        }

        $ManifestPath = Join-Path $RunRoot "manifest.json"
        if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
            Write-Error "INVALID: UE test did not produce $ManifestPath"
            exit 2
        }
        $ManifestPaths += $ManifestPath
    }

    if ($Mode -eq "Milestone") {
        $BundleOutput = Join-Path $SessionRoot "evaluation.json"
        & python $Evaluator --bundle $ManifestPaths --output $BundleOutput
        exit $LASTEXITCODE
    }

    $DevelopmentResults = @()
    foreach ($ManifestPath in $ManifestPaths) {
        $ResultPath = Join-Path (Split-Path -Parent $ManifestPath) "evaluation.json"
        & python $Evaluator --manifest $ManifestPath --output $ResultPath
        $DevelopmentResults += Get-Content -Raw -LiteralPath $ResultPath | ConvertFrom-Json
    }
    [pscustomobject]@{
        schema_version = "physanim-development-run-summary/v1"
        authority = "DEVELOPMENT_ONLY"
        session_root = $SessionRoot
        runs = $DevelopmentResults
    } | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $SessionRoot "development-summary.json") -Encoding utf8
    exit 0
}
finally {
    Pop-Location
}

[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$RuntimeGate = Join-Path $PSScriptRoot "test_runtime.ps1"
$ProductGate = Join-Path $PSScriptRoot "run_causal_standing.ps1"

Push-Location $RepoRoot
try {
    & python -m pytest scripts/tests -q
    if ($LASTEXITCODE -ne 0) {
        throw "Python integrity suite failed with exit code $LASTEXITCODE"
    }

    & $RuntimeGate
    if ($LASTEXITCODE -ne 0) {
        throw "UE runtime tier failed with exit code $LASTEXITCODE"
    }

    & $ProductGate -Mode Milestone
    if ($LASTEXITCODE -ne 0) {
        throw "Causal-standing product tier returned verdict code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}

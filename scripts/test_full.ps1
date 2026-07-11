[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$NodeGate = Join-Path $PSScriptRoot "test_node.ps1"

Push-Location $RepoRoot
try {
    & python -m pytest scripts/tests -q
    if ($LASTEXITCODE -ne 0) {
        throw "Python integrity suite failed with exit code $LASTEXITCODE"
    }

    & $NodeGate
    if ($LASTEXITCODE -ne 0) {
        throw "UE node gate failed with exit code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}

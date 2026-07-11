[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Build = Join-Path $PSScriptRoot "build.ps1"

Push-Location $RepoRoot
try {
    & $Build -Test "PhysAnim.ProductGate"
    if ($LASTEXITCODE -ne 0) {
        throw "PhysAnim.ProductGate failed with exit code $LASTEXITCODE"
    }

    & $Build -Test "PhysAnim.Evidence"
    if ($LASTEXITCODE -ne 0) {
        throw "PhysAnim.Evidence failed with exit code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}

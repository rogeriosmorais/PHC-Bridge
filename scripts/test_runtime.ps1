[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Runner = Join-Path $PSScriptRoot "run_causal_standing.ps1"

Push-Location $RepoRoot
try {
    & $Runner -Mode Development -Variants Normal -AllowDirty
    if ($LASTEXITCODE -ne 0) {
        throw "Normal causal-standing runtime attempt was malformed or blocked with exit code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}

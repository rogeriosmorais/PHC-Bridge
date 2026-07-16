[CmdletBinding()]
param()

# Compatibility alias for older local commands. Runtime evidence is the node gate.
& (Join-Path $PSScriptRoot "test_runtime.ps1")
exit $LASTEXITCODE

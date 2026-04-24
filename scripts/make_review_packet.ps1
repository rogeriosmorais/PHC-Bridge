param(
    [string]$TaskPacket = "",
    [string]$CheckpointPacket = "",
    [string[]]$ExtraPackets = @(),
    [Parameter(Mandatory = $true)]
    [string]$BaseRef,
    [Parameter(Mandatory = $true)]
    [string]$HeadRef,
    [string]$BuildLog = "",
    [string]$TestLog = "",
    [string]$ScopeLog = "",
    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

# Optional mechanical summary generator.
# This script is not a default workflow gate.
# Use it only when the user explicitly asks for a review packet or summary.

$OutDir = Split-Path -Parent $OutputPath
if ($OutDir -and -not (Test-Path $OutDir)) {
    New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
}

$Changed = git diff --name-only $BaseRef $HeadRef
$Stat = git diff --stat $BaseRef $HeadRef

$Lines = @()
$Lines += "# Mechanical Review Summary"
$Lines += ""
$Lines += "Base: `$BaseRef`"
$Lines += "Head: `$HeadRef`"
$Lines += ""
$Lines += "## Packet"
if ($TaskPacket) { $Lines += "- Task packet: `$TaskPacket`" }
if ($CheckpointPacket) { $Lines += "- Checkpoint packet: `$CheckpointPacket`" }
foreach ($Packet in $ExtraPackets) { $Lines += "- Extra packet: `$Packet`" }
$Lines += ""
$Lines += "## Changed Files"
if ($Changed) {
    foreach ($File in $Changed) { $Lines += "- `$File`" }
} else {
    $Lines += "none"
}
$Lines += ""
$Lines += "## Diff Stat"
$Lines += '```text'
$Lines += $Stat
$Lines += '```'
$Lines += ""
$Lines += "## Evidence"
$Lines += "- Build log: $(if ($BuildLog) { "`$BuildLog`" } else { "not provided" })"
$Lines += "- Test log: $(if ($TestLog) { "`$TestLog`" } else { "not provided" })"
$Lines += "- Scope log: $(if ($ScopeLog) { "`$ScopeLog`" } else { "not provided" })"
$Lines += ""
$Lines += "## Note"
$Lines += "This file is optional. Missing review summaries are not product-code blockers."

$Lines | Set-Content -Encoding UTF8 $OutputPath

Write-Host "Wrote optional mechanical summary: $OutputPath"
exit 0

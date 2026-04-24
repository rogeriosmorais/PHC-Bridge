param(
    [string]$TaskPacket = "",
    [string]$CheckpointPacket = "",
    [string[]]$TaskPackets = @(),
    [string]$BaseRef = "",
    [string]$HeadRef = "",
    [switch]$WorkingTree,
    [switch]$AllowExecutionLog,
    [switch]$AllowEvidence
)

function Add-AllowedPath {
    param(
        [System.Collections.Generic.HashSet[string]]$Set,
        [string]$Path
    )

    $Clean = $Path.Trim().Trim("`"").Trim("'").Replace("\", "/")
    if ($Clean.Length -gt 0) {
        [void]$Set.Add($Clean)
    }
}

function Read-AllowedFilesFromPacket {
    param(
        [string]$PacketPath,
        [System.Collections.Generic.HashSet[string]]$Set
    )

    if (-not (Test-Path $PacketPath)) {
        Write-Error "Packet not found: $PacketPath"
        exit 1
    }

    $Lines = Get-Content $PacketPath
    $InAllowed = $false

    foreach ($Line in $Lines) {
        if ($Line -match '^##\s+Allowed Files\s*$') {
            $InAllowed = $true
            continue
        }

        if ($InAllowed -and $Line -match '^##\s+') {
            break
        }

        if ($InAllowed -and $Line -match '^\s*-\s+`([^`]+)`\s*$') {
            Add-AllowedPath $Set $Matches[1]
        }
    }
}

function Read-IncludedPacketsFromCheckpoint {
    param([string]$CheckpointPath)

    if (-not (Test-Path $CheckpointPath)) {
        Write-Error "Checkpoint packet not found: $CheckpointPath"
        exit 1
    }

    $Packets = @()
    foreach ($Line in Get-Content $CheckpointPath) {
        if ($Line -match '`(plans/stage1/20-execution/task-packets/[^`]+\.md)`') {
            $Packets += $Matches[1]
        }
    }

    return $Packets
}

$Allowed = New-Object 'System.Collections.Generic.HashSet[string]'

if ($CheckpointPacket) {
    $Included = Read-IncludedPacketsFromCheckpoint $CheckpointPacket
    foreach ($Packet in $Included) {
        Read-AllowedFilesFromPacket $Packet $Allowed
    }
}

foreach ($Packet in $TaskPackets) {
    Read-AllowedFilesFromPacket $Packet $Allowed
}

if ($TaskPacket) {
    Read-AllowedFilesFromPacket $TaskPacket $Allowed
}

if ($Allowed.Count -eq 0) {
    Write-Error "No allowed files found. Provide -TaskPacket, -TaskPackets, or -CheckpointPacket."
    exit 1
}

if ($AllowExecutionLog) {
    Add-AllowedPath $Allowed "plans/stage1/20-execution/execution-log.md"
}

if ($AllowEvidence) {
    Add-AllowedPath $Allowed "plans/stage1/30-evidence/reviews"
    Add-AllowedPath $Allowed "plans/stage1/30-evidence/blockers"
    Add-AllowedPath $Allowed "plans/stage1/30-evidence/build"
}

$Changed = @()

if ($WorkingTree) {
    $Changed += git diff --name-only HEAD
    $Changed += git ls-files --others --exclude-standard
} else {
    if (-not $BaseRef -or -not $HeadRef) {
        Write-Error "Use either -WorkingTree or provide both -BaseRef and -HeadRef."
        exit 1
    }

    git rev-parse $BaseRef | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Invalid BaseRef: $BaseRef"
        exit 1
    }

    git rev-parse $HeadRef | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Invalid HeadRef: $HeadRef"
        exit 1
    }

    $Changed += git diff --name-only $BaseRef $HeadRef
}

$Changed = $Changed | Where-Object { $_ } | ForEach-Object { $_.Replace("\", "/") } | Sort-Object -Unique

if (-not $Changed) {
    Write-Host "SCOPE CHECK: no changed files."
    exit 0
}

$Violations = @()

foreach ($File in $Changed) {
    $Ok = $false

    foreach ($AllowedPath in $Allowed) {
        if ($File -eq $AllowedPath -or $File.StartsWith($AllowedPath.TrimEnd("/") + "/")) {
            $Ok = $true
            break
        }
    }

    if (-not $Ok) {
        $Violations += $File
    }
}

Write-Host "=== SCOPE CHECK ==="
Write-Host "Changed files:"
$Changed | ForEach-Object { Write-Host "- $_" }

Write-Host ""
Write-Host "Allowed files/prefixes:"
$Allowed | ForEach-Object { Write-Host "- $_" }

if ($Violations.Count -gt 0) {
    Write-Host ""
    Write-Host "SCOPE CHECK: FAILED"
    Write-Host "Forbidden files:"
    $Violations | ForEach-Object { Write-Host "- $_" }
    exit 1
}

Write-Host ""
Write-Host "SCOPE CHECK: PASSED"
exit 0

param(
    [ValidateSet("status", "execute", "review", "fix")]
    [string]$Mode = "status",

    [string]$TaskPacket = "",

    [string]$ExecutionLog = "plans/stage1/20-execution/execution-log.md"
)

if (-not (Test-Path $ExecutionLog)) {
    Write-Error "Execution log not found: $ExecutionLog"
    exit 1
}

$GitStatus = git status --short

$Lines = Get-Content $ExecutionLog

function Get-StateValue {
    param([Parameter(Mandatory=$true)][string]$Key)

    foreach ($Line in $Lines) {
        if (-not $Line.StartsWith("|")) {
            continue
        }

        $Cells = $Line -split "\|"
        if ($Cells.Count -lt 4) {
            continue
        }

        $CellKey = $Cells[1].Trim()
        $CellValue = $Cells[2].Trim()

        if ($CellKey -eq $Key) {
            return $CellValue.Trim().Trim('`').Trim()
        }
    }

    return ""
}

$CurrentTask = Get-StateValue "Current Task ID"
$CurrentTaskPacket = Get-StateValue "Current Task Packet"
$Status = Get-StateValue "Status"
$CurrentCheckpoint = Get-StateValue "Current Checkpoint"

if (-not $TaskPacket) {
    $TaskPacket = $CurrentTaskPacket
}

Write-Host "=== WORKFLOW STATE ==="
Write-Host "Mode: $Mode"
Write-Host "Status: $Status"
Write-Host "Current checkpoint: $CurrentCheckpoint"
Write-Host "Current task: $CurrentTask"
Write-Host "Task packet: $TaskPacket"

if ($GitStatus) {
    Write-Host ""
    Write-Host "Working tree is dirty:"
    $GitStatus
} else {
    Write-Host "Working tree: clean"
}

if ($Mode -eq "execute") {
    if (-not $TaskPacket -or $TaskPacket -eq "none") {
        Write-Error "Cannot execute. Current Task Packet is missing."
        exit 1
    }

    if (-not (Test-Path $TaskPacket)) {
        Write-Error "Cannot execute. Task packet file not found: $TaskPacket"
        exit 1
    }

    if ($GitStatus) {
        Write-Error "Cannot execute from a dirty working tree."
        exit 1
    }

    Write-Host "WORKFLOW CHECK: execute allowed"
    exit 0
}

if ($Mode -eq "review") {
    Write-Host "WORKFLOW CHECK: review is optional; no state-machine gate enforced"
    exit 0
}

if ($Mode -eq "fix") {
    if (-not $TaskPacket -or -not (Test-Path $TaskPacket)) {
        Write-Error "Cannot fix. Task packet file not found."
        exit 1
    }

    Write-Host "WORKFLOW CHECK: fix allowed inside the current task packet"
    exit 0
}

Write-Host "WORKFLOW CHECK: status only"
exit 0

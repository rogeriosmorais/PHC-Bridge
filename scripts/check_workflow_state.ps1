param(
    [ValidateSet("status", "execute", "review", "fix")]
    [string]$Mode = "status",

    [string]$TaskPacket = "",

    [string]$ExecutionLog = "plans/stage1/20-execution/execution-log.md",

    [switch]$Strict
)

if (-not (Test-Path $ExecutionLog)) {
    Write-Error "Execution log not found: $ExecutionLog"
    exit 1
}

$GitStatus = git status --short
$Lines = Get-Content $ExecutionLog

function Normalize-WorkflowValue {
    param([string]$Value)
    if ($null -eq $Value) { return "" }
    return $Value.Trim().Trim('`').Trim()
}

function Get-StateValue {
    param([Parameter(Mandatory=$true)][string]$Key)

    foreach ($Line in $Lines) {
        if (-not $Line.StartsWith("|")) { continue }
        $Cells = $Line -split "\|"
        if ($Cells.Count -lt 4) { continue }

        if ($Cells[1].Trim() -eq $Key) {
            return Normalize-WorkflowValue $Cells[2]
        }
    }

    return ""
}

function Get-SectionText {
    param([Parameter(Mandatory=$true)][string]$Heading)

    $Start = -1
    for ($Index = 0; $Index -lt $Lines.Count; ++$Index) {
        if ($Lines[$Index].Trim() -eq "## $Heading") {
            $Start = $Index + 1
            break
        }
    }

    if ($Start -lt 0) { return "" }

    $End = $Lines.Count
    for ($Index = $Start; $Index -lt $Lines.Count; ++$Index) {
        if ($Lines[$Index] -match '^##\s+') {
            $End = $Index
            break
        }
    }

    if ($End -le $Start) { return "" }
    return ($Lines[$Start..($End - 1)] -join "`n")
}

function Add-Violation {
    param(
        [System.Collections.Generic.List[string]]$Violations,
        [string]$Message
    )
    [void]$Violations.Add($Message)
}

function Extract-CompletedTaskIds {
    param([string]$CompletedTaskCommits)

    $Ids = New-Object 'System.Collections.Generic.HashSet[string]'
    foreach ($Match in [regex]::Matches($CompletedTaskCommits, '([A-Z0-9]+(?:-[A-Z0-9]+)+)\s*=')) {
        [void]$Ids.Add($Match.Groups[1].Value)
    }
    return $Ids
}

function Test-ReferencedCheckpointPacketsExist {
    param([System.Collections.Generic.List[string]]$Violations)

    $CheckpointDir = "plans/stage1/20-execution/checkpoints"
    if (-not (Test-Path $CheckpointDir)) { return }

    foreach ($File in Get-ChildItem -Path $CheckpointDir -Filter "*.md" -File) {
        $Text = Get-Content $File.FullName -Raw
        foreach ($Match in [regex]::Matches($Text, 'plans/stage1/20-execution/task-packets/[^`\s)]+\.md')) {
            $Packet = $Match.Value
            if (-not (Test-Path $Packet)) {
                Add-Violation $Violations "Checkpoint references missing task packet: $Packet ($($File.FullName))"
            }
        }
    }
}

$CurrentTask = Get-StateValue "Current Task ID"
$CurrentTaskPacket = Get-StateValue "Current Task Packet"
$Status = Get-StateValue "Status"
$CurrentCheckpoint = Get-StateValue "Current Checkpoint"
$CompletedTaskCommits = Get-StateValue "Completed Task Commits"
$WorkflowNote = Get-StateValue "Workflow Note"
$NextActionText = Get-SectionText "Next Action"
$NextRunnableTasksText = Get-SectionText "Next Runnable Tasks"
$BlockedDeferredText = Get-SectionText "Blocked / Deferred"

if (-not $TaskPacket) {
    $TaskPacket = $CurrentTaskPacket
}

$Violations = New-Object 'System.Collections.Generic.List[string]'
$CompletedTaskIds = Extract-CompletedTaskIds $CompletedTaskCommits
$ValidIdleStatuses = @("waiting", "blocked", "complete")
$ValidRunnableStatuses = @("runnable")

if (-not $CurrentTask) {
    Add-Violation $Violations "Current Task ID is missing from execution log."
}

if (-not $CurrentTaskPacket) {
    Add-Violation $Violations "Current Task Packet is missing from execution log."
}

if (-not $Status) {
    Add-Violation $Violations "Status is missing from execution log."
}

if ($CurrentTask -eq "none") {
    if ($CurrentTaskPacket -ne "none") {
        Add-Violation $Violations "Current Task ID is none but Current Task Packet is not none: $CurrentTaskPacket"
    }

    if ($ValidIdleStatuses -notcontains $Status) {
        Add-Violation $Violations "Current Task ID is none but Status is not an idle status: $Status"
    }

    if ($NextActionText -match '(?i)\bgo\b' -or $NextActionText -match '(?i)\bexecute\s+[A-Z0-9]+(?:-[A-Z0-9]+)+') {
        Add-Violation $Violations "Current Task ID is none but Next Action still tells the agent to run/execute a task."
    }

    if ($NextRunnableTasksText -match '\|\s*1\s*\|\s*`?(?!none\b)([A-Z0-9]+(?:-[A-Z0-9]+)+)') {
        Add-Violation $Violations "Current Task ID is none but Next Runnable Tasks lists a runnable priority-1 task."
    }
}
else {
    if ($CurrentTaskPacket -eq "none") {
        Add-Violation $Violations "Current Task ID is set but Current Task Packet is none."
    }

    if ($ValidRunnableStatuses -notcontains $Status) {
        Add-Violation $Violations "Current Task ID is set but Status is not runnable: $Status"
    }

    if ($CompletedTaskIds.Contains($CurrentTask)) {
        Add-Violation $Violations "Current task is already listed in Completed Task Commits: $CurrentTask"
    }

    if ($CurrentTaskPacket -ne "none") {
        if (-not (Test-Path $CurrentTaskPacket)) {
            Add-Violation $Violations "Current Task Packet file does not exist: $CurrentTaskPacket"
        }

        $PacketLeaf = Split-Path $CurrentTaskPacket -Leaf
        if ($PacketLeaf -notlike "$CurrentTask*") {
            Add-Violation $Violations "Current Task Packet filename does not start with Current Task ID. Task=$CurrentTask Packet=$PacketLeaf"
        }
    }

    if ($NextActionText -notmatch '(?i)\bgo\b') {
        Add-Violation $Violations "Current task is runnable but Next Action does not say go."
    }

    if ($NextActionText -notmatch [regex]::Escape($CurrentTask)) {
        Add-Violation $Violations "Next Action does not reference the current task ID: $CurrentTask"
    }

    foreach ($CompletedId in $CompletedTaskIds) {
        if ($NextActionText -match [regex]::Escape($CompletedId)) {
            Add-Violation $Violations "Next Action references an already completed task: $CompletedId"
        }
    }
}

if ($CurrentCheckpoint -match 'COMPLETE' -and $BlockedDeferredText -match 'blocked until Slice 1 pure support logic is green') {
    Add-Violation $Violations "Blocked / Deferred table is stale: it still says runtime work is blocked until Slice 1 is green even though checkpoint is COMPLETE."
}

Test-ReferencedCheckpointPacketsExist $Violations

Write-Host "=== WORKFLOW STATE ==="
Write-Host "Mode: $Mode"
Write-Host "Strict: $($Strict.IsPresent -or $Mode -eq 'execute')"
Write-Host "Status: $Status"
Write-Host "Current checkpoint: $CurrentCheckpoint"
Write-Host "Current task: $CurrentTask"
Write-Host "Task packet: $TaskPacket"
Write-Host "Workflow note: $WorkflowNote"

if ($GitStatus) {
    Write-Host ""
    Write-Host "Working tree is dirty:"
    $GitStatus
} else {
    Write-Host "Working tree: clean"
}

if ($Violations.Count -gt 0) {
    Write-Host ""
    Write-Host "WORKFLOW INVARIANTS: FAILED"
    foreach ($Violation in $Violations) {
        Write-Host "- $Violation"
    }

    if ($Strict -or $Mode -eq "execute") {
        exit 1
    }
} else {
    Write-Host ""
    Write-Host "WORKFLOW INVARIANTS: PASSED"
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

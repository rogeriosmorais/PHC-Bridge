param(
    [Parameter(Mandatory=$true)]
    [string]$CompletedTask,

    [Parameter(Mandatory=$true)]
    [string]$Commit,

    [Parameter(Mandatory=$true)]
    [string]$NextTask,

    [Parameter(Mandatory=$true)]
    [string]$NextPacket,

    [ValidateSet("runnable", "waiting", "blocked", "complete")]
    [string]$Status = "runnable",

    [string]$CurrentCheckpoint = "",

    [string]$LastBuild = "",

    [string]$LastTest = "",

    [string]$LastScope = "",

    [string]$WorkflowNote = "",

    [string]$ExecutionLog = "plans/stage1/20-execution/execution-log.md"
)

if (-not (Test-Path $ExecutionLog)) {
    Write-Error "Execution log not found: $ExecutionLog"
    exit 1
}

function Escape-BacktickValue {
    param([string]$Value)

    if ($null -eq $Value) {
        return ""
    }

    return ($Value -replace '`', '')
}

function Normalize-None {
    param([string]$Value)

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return "none"
    }

    return $Value.Trim()
}

$NextTask = Normalize-None $NextTask
$NextPacket = Normalize-None $NextPacket

if ($NextTask -eq "none" -and $NextPacket -ne "none") {
    Write-Error "NextTask is none but NextPacket is not none."
    exit 1
}

if ($NextTask -ne "none" -and $NextPacket -eq "none") {
    Write-Error "NextTask is set but NextPacket is none."
    exit 1
}

if ($NextTask -ne "none" -and -not (Test-Path $NextPacket)) {
    Write-Error "Next task packet does not exist: $NextPacket"
    exit 1
}

$Lines = [System.Collections.Generic.List[string]]::new()
foreach ($Line in Get-Content $ExecutionLog) {
    [void]$Lines.Add($Line)
}

function Get-StateValue {
    param([string]$Key)

    foreach ($Line in $Lines) {
        if (-not $Line.StartsWith("|")) {
            continue
        }

        $Cells = $Line -split "\|"
        if ($Cells.Count -lt 4) {
            continue
        }

        if ($Cells[1].Trim() -eq $Key) {
            return $Cells[2].Trim().Trim('`').Trim()
        }
    }

    return ""
}

function Set-StateValue {
    param(
        [string]$Key,
        [string]$Value
    )

    $CleanValue = Escape-BacktickValue $Value

    for ($Index = 0; $Index -lt $Lines.Count; ++$Index) {
        $Line = $Lines[$Index]
        if (-not $Line.StartsWith("|")) {
            continue
        }

        $Cells = $Line -split "\|"
        if ($Cells.Count -lt 4) {
            continue
        }

        if ($Cells[1].Trim() -eq $Key) {
            $Lines[$Index] = "| $Key | ``$CleanValue`` |"
            return
        }
    }

    Write-Error "State field not found: $Key"
    exit 1
}

function Replace-Section {
    param(
        [string]$Heading,
        [string]$Replacement
    )

    $Start = -1
    for ($Index = 0; $Index -lt $Lines.Count; ++$Index) {
        if ($Lines[$Index].Trim() -eq "## $Heading") {
            $Start = $Index
            break
        }
    }

    if ($Start -lt 0) {
        Write-Error "Section not found: ## $Heading"
        exit 1
    }

    $End = $Lines.Count
    for ($Index = $Start + 1; $Index -lt $Lines.Count; ++$Index) {
        if ($Lines[$Index] -match '^##\s+') {
            $End = $Index
            break
        }
    }

    $NewBlock = @("## $Heading") + ($Replacement -split "`r?`n")
    $Lines.RemoveRange($Start, $End - $Start)
    $Lines.InsertRange($Start, [string[]]$NewBlock)
}

$CompletedCommits = Get-StateValue "Completed Task Commits"

if ($CompletedTask -ne "none" -and $Commit -ne "none") {
    if ($CompletedCommits -notmatch [regex]::Escape("$CompletedTask =")) {
        if ([string]::IsNullOrWhiteSpace($CompletedCommits) -or $CompletedCommits -eq "none") {
            $CompletedCommits = "$CompletedTask = $Commit"
        } else {
            $CompletedCommits = "$CompletedCommits; $CompletedTask = $Commit"
        }

        Set-StateValue "Completed Task Commits" $CompletedCommits
    }

    Set-StateValue "Latest Technical Head" $Commit
}

Set-StateValue "Current Task ID" $NextTask
Set-StateValue "Current Task Packet" $NextPacket
Set-StateValue "Status" $Status

if ($CurrentCheckpoint) {
    Set-StateValue "Current Checkpoint" $CurrentCheckpoint
}

if ($LastBuild) {
    Set-StateValue "Last Build" $LastBuild
}

if ($LastTest) {
    Set-StateValue "Last Test" $LastTest
}

if ($LastScope) {
    Set-StateValue "Last Scope" $LastScope
}

if ($WorkflowNote) {
    Set-StateValue "Workflow Note" $WorkflowNote
}

if ($NextTask -eq "none") {
    $Reason = if ($WorkflowNote) { $WorkflowNote } else { "No implementation task is currently runnable." }

    Replace-Section "Next Action" @"
No implementation task is currently runnable.

Reason:

```text
$Reason
```
"@

    Replace-Section "Next Runnable Tasks" @"
| Priority | Task ID | Packet | Notes |
|---|---|---|---|
| 1 | `none` | `none` | No task is currently runnable. |
"@
}
else {
    Replace-Section "Next Action" @"
Run:

```text
go
```

Meaning:

```text
execute $NextTask only
```
"@

    Replace-Section "Next Runnable Tasks" @"
| Priority | Task ID | Packet | Notes |
|---|---|---|---|
| 1 | `$NextTask` | `$NextPacket` | Current runnable task. |
"@
}

Set-Content -Path $ExecutionLog -Value $Lines -Encoding UTF8

& .\scripts\check_workflow_state.ps1 -Mode status -Strict -ExecutionLog $ExecutionLog
if ($LASTEXITCODE -ne 0) {
    Write-Error "Updated execution log failed strict workflow validation."
    exit $LASTEXITCODE
}

Write-Host "TASK COMPLETION TRANSITION: updated $ExecutionLog"
exit 0

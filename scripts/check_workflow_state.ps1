param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("execute", "review", "fix", "accept")]
    [string]$Mode,

    [Parameter(Mandatory = $true)]
    [string]$Checkpoint,

    [string]$ReviewReport = "",

    [string]$ExecutionLog = "plans/stage1/20-execution/execution-log.md"
)

if (-not (Test-Path $ExecutionLog)) {
    Write-Error "Execution log not found: $ExecutionLog"
    exit 1
}

$GitStatus = git status --short
if ($GitStatus) {
    Write-Error "Working tree is dirty. Agents must start from a clean tree."
    $GitStatus
    exit 1
}

$Lines = Get-Content $ExecutionLog

function Get-StateValue {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Key
    )

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

$CurrentCheckpoint = Get-StateValue "Checkpoint ID"
$CheckpointStatus = Get-StateValue "Checkpoint Status"
$CurrentTask = Get-StateValue "Current Task ID"
$ReviewPacket = Get-StateValue "Review Packet"
$ReviewVerdict = Get-StateValue "Review Verdict"
$ScopeCheck = Get-StateValue "Scope Check"
$ScopeLog = Get-StateValue "Scope Log"
$BuildLog = Get-StateValue "Build Log"
$TestLog = Get-StateValue "Test Log"
$WorkingTree = Get-StateValue "Working Tree"
$BlockingReason = Get-StateValue "Blocking Reason"

if ($CurrentCheckpoint -ne $Checkpoint) {
    Write-Error "Checkpoint mismatch. Requested '$Checkpoint', execution-log has '$CurrentCheckpoint'."
    exit 1
}

Write-Host "=== WORKFLOW STATE ==="
Write-Host "Checkpoint: $CurrentCheckpoint"
Write-Host "Status: $CheckpointStatus"
Write-Host "Current task: $CurrentTask"
Write-Host "Review packet: $ReviewPacket"
Write-Host "Review verdict: $ReviewVerdict"
Write-Host "Scope check: $ScopeCheck"
Write-Host "Scope log: $ScopeLog"
Write-Host "Build log: $BuildLog"
Write-Host "Test log: $TestLog"
Write-Host "Working tree: $WorkingTree"
Write-Host "Blocking reason: $BlockingReason"
Write-Host ""

switch ($Mode) {
    "execute" {
        if ($CheckpointStatus -ne "in-progress") {
            Write-Error "Cannot execute. Checkpoint status must be 'in-progress', got '$CheckpointStatus'."
            exit 1
        }

        if (-not $CurrentTask -or $CurrentTask -eq "none") {
            Write-Error "Cannot execute. Current Task ID is missing."
            exit 1
        }

        Write-Host "WORKFLOW PREFLIGHT: execute allowed"
        exit 0
    }

    "review" {
        if ($CheckpointStatus -ne "review-pending") {
            Write-Error "Cannot review. Checkpoint status must be 'review-pending', got '$CheckpointStatus'."
            exit 1
        }

        if (-not $ScopeCheck -or $ScopeCheck -eq "missing") {
            Write-Error "Cannot review. Scope Check field is missing."
            exit 1
        }

        if (-not $ReviewPacket -or $ReviewPacket -eq "none" -or $ReviewPacket -eq "missing") {
            Write-Error "Cannot review. Review Packet path is missing."
            exit 1
        }

        if (-not (Test-Path $ReviewPacket)) {
            Write-Error "Cannot review. Review Packet file not found: $ReviewPacket"
            exit 1
        }

        if (-not $ScopeLog -or $ScopeLog -eq "none" -or $ScopeLog -eq "missing") {
            Write-Error "Cannot review. Scope Log path is missing."
            exit 1
        }

        if (-not (Test-Path $ScopeLog)) {
            Write-Error "Cannot review. Scope Log file not found: $ScopeLog"
            exit 1
        }

        if (-not $BuildLog -or $BuildLog -eq "none" -or $BuildLog -eq "missing") {
            Write-Error "Cannot review. Build Log path is missing."
            exit 1
        }

        if (-not (Test-Path $BuildLog)) {
            Write-Error "Cannot review. Build Log file not found: $BuildLog"
            exit 1
        }

        Write-Host "WORKFLOW PREFLIGHT: review allowed"
        exit 0
    }

    "fix" {
        if ($CheckpointStatus -ne "fix-required" -and $CheckpointStatus -ne "blocked") {
            Write-Error "Cannot fix. Checkpoint status must be 'fix-required' or 'blocked', got '$CheckpointStatus'."
            exit 1
        }

        if ($ReviewVerdict -eq "reject" -and $CheckpointStatus -eq "fix-required") {
            Write-Host "WORKFLOW PREFLIGHT: fix allowed after evidence rejection"
            Write-Host "Fix scope: repair evidence/range/review packet unless implementation rejection requires rollback."
            exit 0
        }

        Write-Host "WORKFLOW PREFLIGHT: fix allowed"
        exit 0
    }

    "accept" {
        if ($CheckpointStatus -ne "review-pending") {
            Write-Error "Cannot accept. Checkpoint status must be 'review-pending', got '$CheckpointStatus'."
            exit 1
        }

        if (-not $ReviewReport) {
            Write-Error "Cannot accept. ReviewReport path is required for accept mode."
            exit 1
        }

        if (-not (Test-Path -LiteralPath $ReviewReport)) {
            Write-Error "Cannot accept. ReviewReport file not found: $ReviewReport"
            exit 1
        }

        $ReviewText = Get-Content -LiteralPath $ReviewReport -Raw

        if ($ReviewText -notmatch "## Verdict") {
            Write-Error "Cannot accept. Review report missing verdict section."
            exit 1
        }

        if ($ReviewText -notmatch "(?im)^`?accept`?\s*$") {
            Write-Error "Cannot accept. Review report verdict is not accept."
            exit 1
        }

        if ($ReviewText -notmatch "(?is)## Blockers\s+none") {
            Write-Error "Cannot accept. Review report blockers are not none."
            exit 1
        }

        Write-Host "WORKFLOW PREFLIGHT: accept allowed"
        exit 0
    }
}
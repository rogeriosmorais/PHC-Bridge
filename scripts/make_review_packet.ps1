param(
    [Parameter(Mandatory=$true)]
    [string]$TaskPacket,

    [Parameter(Mandatory=$true)]
    [string]$BaseRef,

    [Parameter(Mandatory=$true)]
    [string]$HeadRef,

    [string]$BuildLog = "",

    [string]$TestLog = "",

    [int]$MaxDiffLines = 800,

    [int]$MaxLogLines = 200,

    [switch]$FullDiff
)

if (-not (Test-Path $TaskPacket)) {
    Write-Error "Task packet not found: $TaskPacket"
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

$ChangedFiles = git diff --name-only $BaseRef $HeadRef
if (-not $ChangedFiles) {
    Write-Error "No diff found between BaseRef and HeadRef."
    exit 1
}

Write-Host "=== REVIEW PACKET ==="
Write-Host ""

Write-Host "Branch:"
git branch --show-current
Write-Host ""

Write-Host "Head:"
git rev-parse $HeadRef
Write-Host ""

Write-Host "Base:"
git rev-parse $BaseRef
Write-Host ""

Write-Host "Review target:"
Write-Host "Task packet: $TaskPacket"
Write-Host "Task base: $BaseRef"
Write-Host "Task head: $HeadRef"
Write-Host "Commit: $HeadRef"
Write-Host ""

Write-Host "=== TASK PACKET CONTENT ==="
Get-Content $TaskPacket
Write-Host ""

Write-Host "=== CHANGED FILES ==="
$ChangedFiles
Write-Host ""

Write-Host "=== DIFF STAT ==="
git diff --stat $BaseRef $HeadRef
Write-Host ""

Write-Host "=== DIFF ==="
$DiffLines = git diff $BaseRef $HeadRef
if ($FullDiff) {
    $DiffLines
} else {
    $DiffLines | Select-Object -First $MaxDiffLines
    if ($DiffLines.Count -gt $MaxDiffLines) {
        Write-Host ""
        Write-Host "[diff truncated after $MaxDiffLines lines; rerun with -FullDiff if needed]"
    }
}
Write-Host ""

Write-Host "=== BUILD OUTPUT ==="
if ($BuildLog -and (Test-Path $BuildLog)) {
    Get-Content $BuildLog | Select-Object -Last $MaxLogLines
} else {
    Write-Host "No build log supplied."
}
Write-Host ""

Write-Host "=== TEST OUTPUT ==="
if ($TestLog -and (Test-Path $TestLog)) {
    Get-Content $TestLog | Select-Object -Last $MaxLogLines
} else {
    Write-Host "No test log supplied."
}
Write-Host ""

Write-Host "=== REVIEW INSTRUCTION ==="
Write-Host "Review this diff only against the task packet above."
Write-Host "Do not review architecture."
Write-Host "Return at most 3 blockers, 3 non-blocking nits, verdict, and next action."

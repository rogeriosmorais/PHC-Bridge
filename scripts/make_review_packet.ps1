param(
    [Parameter(Mandatory=$false)]
    [string]$TaskPacket = "",

    [Parameter(Mandatory=$false)]
    [string]$CheckpointPacket = "",

    [Parameter(Mandatory=$false)]
    [string[]]$ExtraPackets = @(),

    [Parameter(Mandatory=$true)]
    [string]$BaseRef,

    [Parameter(Mandatory=$true)]
    [string]$HeadRef,

    [Parameter(Mandatory=$true)]
    [string]$OutputPath,

    [string]$BuildLog = "",

    [string]$TestLog = "",

    [int]$MaxDiffLines = 800,

    [int]$MaxLogLines = 200,

    [switch]$FullDiff
)

if (-not $TaskPacket -and -not $CheckpointPacket) {
    Write-Error "Either -TaskPacket or -CheckpointPacket must be specified."
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

$OutputDir = Split-Path -Parent $OutputPath
if ($OutputDir -and -not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

$Lines = New-Object System.Collections.Generic.List[string]

function Add-Line {
    param([string]$Text = "")
    $script:Lines.Add($Text)
}

Add-Line "=== REVIEW PACKET ==="
Add-Line ""

Add-Line "Branch:"
Add-Line (git branch --show-current)
Add-Line ""

Add-Line "Head:"
Add-Line (git rev-parse $HeadRef)
Add-Line ""

Add-Line "Base:"
Add-Line (git rev-parse $BaseRef)
Add-Line ""

Add-Line "Review target:"
if ($CheckpointPacket) { Add-Line "Checkpoint packet: $CheckpointPacket" }
if ($TaskPacket) { Add-Line "Task packet: $TaskPacket" }
Add-Line "Task base: $BaseRef"
Add-Line "Task head: $HeadRef"
Add-Line "Commit: $HeadRef"
Add-Line ""

if ($CheckpointPacket) {
    Add-Line "=== CHECKPOINT PACKET CONTENT ==="
    if (Test-Path $CheckpointPacket) {
        Get-Content $CheckpointPacket | ForEach-Object { Add-Line $_ }
    } else {
        Add-Line "Checkpoint packet not found: $CheckpointPacket"
    }
    Add-Line ""
}

if ($TaskPacket) {
    Add-Line "=== TASK PACKET CONTENT ==="
    if (Test-Path $TaskPacket) {
        Get-Content $TaskPacket | ForEach-Object { Add-Line $_ }
    } else {
        Add-Line "Task packet not found: $TaskPacket"
    }
    Add-Line ""
}

foreach ($Extra in $ExtraPackets) {
    Add-Line "=== EXTRA PACKET CONTENT: $Extra ==="
    if (Test-Path $Extra) {
        Get-Content $Extra | ForEach-Object { Add-Line $_ }
    } else {
        Add-Line "Extra packet not found: $Extra"
    }
    Add-Line ""
}

Add-Line "=== CHANGED FILES ==="
$ChangedFiles | ForEach-Object { Add-Line $_ }
Add-Line ""

Add-Line "=== DIFF STAT ==="
git diff --stat $BaseRef $HeadRef | ForEach-Object { Add-Line $_ }
Add-Line ""

Add-Line "=== DIFF ==="
$DiffLines = git diff $BaseRef $HeadRef
if ($FullDiff) {
    $DiffLines | ForEach-Object { Add-Line $_ }
} else {
    $DiffLines | Select-Object -First $MaxDiffLines | ForEach-Object { Add-Line $_ }
    if ($DiffLines.Count -gt $MaxDiffLines) {
        Add-Line ""
        Add-Line "[diff truncated after $MaxDiffLines lines; rerun with -FullDiff if needed]"
    }
}
Add-Line ""

Add-Line "=== BUILD OUTPUT ==="
if ($BuildLog -and (Test-Path $BuildLog)) {
    Get-Content $BuildLog | Select-Object -Last $MaxLogLines | ForEach-Object { Add-Line $_ }
} else {
    Add-Line "No build log supplied."
}
Add-Line ""

Add-Line "=== TEST OUTPUT ==="
if ($TestLog -and (Test-Path $TestLog)) {
    Get-Content $TestLog | Select-Object -Last $MaxLogLines | ForEach-Object { Add-Line $_ }
} else {
    Add-Line "No test log supplied."
}
Add-Line ""

Add-Line "=== REVIEW INSTRUCTION ==="
Add-Line "Review this packet only using plans/stage1/20-execution/task-packets/REVIEWER_PROMPT.md."
Add-Line "Do not review architecture."
Add-Line "Do not edit files."
Add-Line "Return a structured review report."

$Lines | Set-Content -Path $OutputPath -Encoding UTF8

Write-Host "Review packet written to:"
Write-Host $OutputPath

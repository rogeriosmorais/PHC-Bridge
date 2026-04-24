param(
    [Parameter(Mandatory=$true)]
    [string]$TaskPacket,

    [Parameter(Mandatory=$true)]
    [string]$BaseRef,

    [Parameter(Mandatory=$true)]
    [string]$HeadRef,

    [string]$BuildLog = "",

    [string]$TestLog = ""
)

Write-Host "=== CURRENT TASK REVIEW STARTUP ==="
Write-Host ""

Write-Host "Reviewer must read:"
Write-Host "plans/stage1/20-execution/task-packets/REVIEWER_PROMPT.md"
Write-Host ""

Write-Host "Reviewer must NOT edit:"
Write-Host "- plans/stage1/20-execution/execution-log.md"
Write-Host "- task packets"
Write-Host "- production code"
Write-Host "- tests"
Write-Host "- planning docs"
Write-Host ""

Write-Host "Generate review packet with:"
Write-Host ".\scripts\make_review_packet.ps1 -TaskPacket $TaskPacket -BaseRef $BaseRef -HeadRef $HeadRef -BuildLog $BuildLog -TestLog $TestLog"
Write-Host ""

Write-Host "Reviewer instruction:"
Write-Host "Review current task using only REVIEWER_PROMPT.md and the generated review packet."
Write-Host "Return a structured review report."
Write-Host "Do not update execution-log.md."

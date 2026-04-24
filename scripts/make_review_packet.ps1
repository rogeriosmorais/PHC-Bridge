param(
    [Parameter(Mandatory=$true)]
    [string]$TaskPacket,

    [string]$BaseRef = "HEAD~1",

    [string]$HeadRef = "HEAD"
)

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

Write-Host "Task packet:"
Write-Host $TaskPacket
Write-Host ""

Write-Host "=== TASK PACKET CONTENT ==="
Get-Content $TaskPacket
Write-Host ""

Write-Host "=== CHANGED FILES ==="
git diff --name-only $BaseRef $HeadRef
Write-Host ""

Write-Host "=== DIFF STAT ==="
git diff --stat $BaseRef $HeadRef
Write-Host ""

Write-Host "=== FULL DIFF ==="
git diff $BaseRef $HeadRef
Write-Host ""

Write-Host "=== REVIEW INSTRUCTION ==="
Write-Host "Review this diff only against the task packet above."
Write-Host "Do not review architecture."
Write-Host "Return at most 3 blockers, 3 non-blocking nits, verdict, and next action."

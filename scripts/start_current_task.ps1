Write-Host "=== CURRENT TASK STARTUP ==="
Write-Host ""

Write-Host "Task base:"
git rev-parse HEAD
Write-Host ""

Write-Host "Read:"
Write-Host "1. AGENTS.md"
Write-Host "2. plans/stage1/20-execution/agent_workflow_protocol.md"
Write-Host "3. plans/stage1/20-execution/execution-log.md"
Write-Host "4. plans/stage1/20-execution/task-packets/IMPLEMENTER_PROMPT.md"
Write-Host "5. the current task packet listed in execution-log.md"
Write-Host ""

Write-Host "Shortcut instruction for the agent:"
Write-Host ""
Write-Host "go"
Write-Host ""
Write-Host "Meaning:"
Write-Host "Execute the current task packet only. Record task base before edits. Commit only after required build/tests pass. Generate or prepare review. Do not continue to the next task."

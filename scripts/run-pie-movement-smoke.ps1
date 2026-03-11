[CmdletBinding()]
param(
    [string]$EngineRoot = $(if ($env:UE5_PATH) { $env:UE5_PATH } else { "E:\UE_5.7\Engine" }),
    [string]$ProjectPath = "F:\NewEngine\PhysAnimUE5\PhysAnimUE5.uproject"
)

& "$PSScriptRoot\run-pie-smoke.ps1" -EngineRoot $EngineRoot -ProjectPath $ProjectPath -TestName "PhysAnim.PIE.MovementSmoke"
exit $LASTEXITCODE

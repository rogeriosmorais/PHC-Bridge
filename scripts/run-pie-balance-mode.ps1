[CmdletBinding()]
param(
    [string]$EngineRoot = $(if ($env:UE5_PATH) { $env:UE5_PATH } else { "E:\UE_5.7\Engine" }),
    [string]$ProjectPath = "F:\NewEngine\PhysAnimUE5\PhysAnimUE5.uproject"
)

$scriptPath = Join-Path $PSScriptRoot "run-pie-smoke.ps1"
powershell.exe -File $scriptPath -EngineRoot $EngineRoot -ProjectPath $ProjectPath -TestName "PhysAnim.PIE.BalanceMode"

[CmdletBinding()]
param(
    [string]$EngineRoot = $(if ($env:UE5_PATH) { $env:UE5_PATH } else { "E:\UE_5.7\Engine" }),
    [string]$ProjectPath = "F:\NewEngine\PhysAnimUE5\PhysAnimUE5.uproject",
    [string]$TestName = "PhysAnim.PIE.Smoke",
    [string]$PreExecCmds = "",
    [switch]$UseEditorExe
)

$editorName = if ($UseEditorExe) { "UnrealEditor.exe" } else { "UnrealEditor-Cmd.exe" }
$editorPath = Join-Path $EngineRoot "Binaries\Win64\$editorName"
$projectRoot = Split-Path -Parent $ProjectPath
$projectName = [System.IO.Path]::GetFileNameWithoutExtension($ProjectPath)
$savedLogsDir = Join-Path $projectRoot "Saved\Logs"

if (-not (Test-Path $editorPath)) {
    throw "Unreal editor executable not found at '$editorPath'."
}

if (-not (Test-Path $ProjectPath)) {
    throw "Project file not found at '$ProjectPath'."
}

function Split-ExecCommands {
    param(
        [string]$CommandText
    )

    if ([string]::IsNullOrWhiteSpace($CommandText)) {
        return @()
    }

    return @(
        $CommandText `
            -split "[;\r\n]+" |
            ForEach-Object { $_.Trim() } |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    )
}

$execCmdParts = @()
if (-not [string]::IsNullOrWhiteSpace($PreExecCmds)) {
    $execCmdParts += Split-ExecCommands -CommandText $PreExecCmds
}
$execCmdParts += ('Automation RunTests {0}' -f $TestName)
$execCmdParts += 'Quit'
$execCmds = [string]::Join(', ', $execCmdParts)

$arguments = @(
    $ProjectPath,
    ('-ExecCmds={0}' -f $execCmds),
    '-TestExit=Automation Test Queue Empty',
    '-Unattended',
    '-NoSound',
    '-NoSplash'
)

Write-Host "Running $TestName with $editorPath"
Write-Host "ExecCmds: $execCmds"
& $editorPath @arguments
$exitCode = $LASTEXITCODE

$latestLog = $null
if (Test-Path $savedLogsDir) {
    $latestLog = Get-ChildItem -Path $savedLogsDir -File -Filter "$projectName*.log" |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
}

Write-Host "Editor exit code: $exitCode"
if ($latestLog) {
    Write-Host "Review log: $($latestLog.FullName)"
}

exit $exitCode

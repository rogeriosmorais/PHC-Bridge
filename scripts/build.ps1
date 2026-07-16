param (
    [switch]$Full,  # Perform full plugin packaging (Slow)
    [switch]$Run,   # Launch the Editor on success
    [switch]$Clean,  # Wipe Binaries/Intermediate before building
    [switch]$CloseEngineProcesses, # Opt-in: stop only UE 5.7 processes owned by UE5_PATH
    [string]$Test,    # Optional Unreal automation test name to run after a successful build
    [switch]$SkipBuild,
    [ValidateSet("NullRHI", "RenderOffscreen")]
    [string]$TestMode = "NullRHI",
    [string]$ReportExportPath,
    [string]$ProductRunRoot,
    [string]$ProductRunId,
    [string]$ProductVariant,
    [int]$ProductRepetition = 0,
    [string]$SourceCommit,
    [string]$ModelOnnxSha256,
    [bool]$SourceTreeDirty = $false,
    [switch]$ActionSemanticTrace,
    [switch]$MannyLocalFrameRoundtripTrace,
    [switch]$ExperimentalComponentActionAxis,
    [switch]$ExperimentalComponentActionAxisFromFirstPolicy,
    [switch]$ExperimentalBindNeutralFromFirstPolicy,
    [switch]$ExperimentalConstraintRangeRemapBypassFromFirstPolicy,
    [string]$ExperimentalActionFamily,
    [int]$ExperimentalActionJointStart = -1,
    [int]$ExperimentalActionJointCount = 0,
    [switch]$ExperimentalPolicyActionBaselineResidual,
    [switch]$ExperimentalPolicyActionZeroUntilBaseline,
    [switch]$ExperimentalPhysicsBodyObservationPositions,
    [double]$ExperimentalActiveStrengthFactor = 1.0,
    [switch]$ExperimentalCheckpointTorqueCeiling,
    [switch]$ExperimentalCheckpointForcePd,
    [switch]$PolicyInputProvenanceTrace,
    [switch]$StartupChronologyTrace
)

# 1. Environment Setup
if (-not $env:UE5_PATH) {
    Write-Host "Environment variables not found. Loading local paths..." -ForegroundColor Cyan
    . "$PSScriptRoot\local.paths.ps1"
}

$UnrealProcessSafetyModule = Join-Path $PSScriptRoot "UnrealProcessSafety.psm1"
Import-Module $UnrealProcessSafetyModule -Force -ErrorAction Stop
$EngineVersion = Assert-UnrealEngineVersion `
    -EngineRoot $env:UE5_PATH `
    -ExpectedMajorVersion 5 `
    -ExpectedMinorVersion 7

$ProjectDir = "$PWD\PhysAnimUE5"
$ProjectFile = "$ProjectDir\PhysAnimUE5.uproject"
$PluginDir = "$ProjectDir\Plugins\PhysAnimPlugin"
$PluginFile = "$PluginDir\PhysAnimPlugin.uplugin"
$PackageDir = "$PWD\_build\PhysAnimPlugin"
$EditorExe = "$env:UE5_PATH\Binaries\Win64\UnrealEditor.exe"
$EditorCmdExe = "$env:UE5_PATH\Binaries\Win64\UnrealEditor-Cmd.exe"

$Stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

# 2. Protect unrelated Unreal installations and user sessions.
Write-Host "--- Checking for UE 5.7 processes owned by $($EngineVersion.EngineRoot) ---" -ForegroundColor Yellow
$EngineOwnedProcesses = @(
    Get-EngineOwnedUnrealProcesses -EngineRoot $EngineVersion.EngineRoot
)

if ($EngineOwnedProcesses.Count -gt 0) {
    $ProcessSummary = ($EngineOwnedProcesses | ForEach-Object {
        "$($_.ProcessName):$($_.Id) [$($_.Path)]"
    }) -join ", "

    if (-not $CloseEngineProcesses) {
        Write-Error (
            "BLOCKED: UE 5.7 processes from this project's configured engine are running: " +
            "$ProcessSummary. Close them manually or rerun with -CloseEngineProcesses. " +
            "Processes outside $($EngineVersion.EngineRoot), including Unreal Engine 5.8, are ignored and never stopped."
        )
        exit 3
    }

    Write-Host "Closing only UE 5.7 processes owned by the configured engine root..." -ForegroundColor Magenta
    $StoppedProcesses = @(
        Stop-EngineOwnedUnrealProcesses `
            -EngineRoot $EngineVersion.EngineRoot `
            -ExpectedMajorVersion 5 `
            -ExpectedMinorVersion 7
    )
    foreach ($Process in $StoppedProcesses) {
        Write-Host "Closed $($Process.ProcessName):$($Process.Id) [$($Process.Path)]" -ForegroundColor Magenta
    }
    Start-Sleep -Seconds 1 # Wait for OS to release file handles.
}

# 3. Optional Cleanup
if ($Clean) {
    Write-Host "--- Cleaning Build Artifacts ---" -ForegroundColor Cyan
    $DirsToClean = @(
        "$ProjectDir\Binaries", "$ProjectDir\Intermediate",
        "$PluginDir\Binaries", "$PluginDir\Intermediate"
    )
    foreach ($Dir in $DirsToClean) {
        if (Test-Path $Dir) { 
            Write-Host "Removing $Dir..." -ForegroundColor Gray
            Remove-Item -Recurse -Force $Dir 
        }
    }
}

Write-Host "--- Starting Build (Mode: $(if($Full){"Full Package"}else{"Fast Iteration"})) ---" -ForegroundColor Yellow

# 4. Compile Step (Default Path)
if (-not $SkipBuild) {
    Write-Host "[1] Compiling Editor Binaries..." -ForegroundColor Green
    & "$env:UE5_PATH\Build\BatchFiles\Build.bat" PhysAnimUE5Editor Win64 Development `
        -Project="$ProjectFile" `
        -Progress -NoHotReloadFromIDE

    if ($LASTEXITCODE -ne 0) {
        Write-Host "!!! COMPILATION FAILED !!!" -ForegroundColor Red
        exit $LASTEXITCODE
    }
}
else {
    Write-Host "[1] Reusing existing Editor binaries." -ForegroundColor Green
}

# 5. Full Packaging (Only if -Full is passed)
if ($Full) {
    if ($SkipBuild) {
        throw "-Full cannot be combined with -SkipBuild."
    }
    Write-Host "[2] Packaging Plugin..." -ForegroundColor Green
    if (Test-Path $PackageDir) { Remove-Item -Recurse -Force $PackageDir }

    & "$env:UE5_PATH\Build\BatchFiles\RunUAT.bat" BuildPlugin `
        -Plugin="$PluginFile" `
        -Package="$PackageDir" `
        -TargetPlatforms=Win64 `
        -Rocket -Force
    
    if ($LASTEXITCODE -ne 0) { 
        Write-Host "!!! PACKAGING FAILED !!!" -ForegroundColor Red
        exit $LASTEXITCODE 
    }
}

# 6. Optional Automation Test Run
if ($Test) {
    Write-Host "[3] Running Automation Test: $Test" -ForegroundColor Green
    $EditorArguments = @(
        $ProjectFile,
        "-ExecCmds=Automation RunTests $Test; Quit",
        "-TestExit=Automation Test Queue Empty",
        "-NoSound",
        "-Unattended",
        "-Log"
    )
    if ($TestMode -eq "RenderOffscreen") {
        $EditorArguments += "-RenderOffscreen"
    }
    else {
        $EditorArguments += "-NullRHI"
    }
    if ($ReportExportPath) {
        $EditorArguments += "-ReportExportPath=$ReportExportPath"
    }
    if ($ProductRunRoot) {
        $EditorArguments += "-PhysAnimProductRunRoot=$ProductRunRoot"
    }
    if ($ProductRunId) {
        $EditorArguments += "-PhysAnimProductRunId=$ProductRunId"
    }
    if ($ProductVariant) {
        $EditorArguments += "-PhysAnimProductVariant=$ProductVariant"
    }
    if ($ProductRepetition -gt 0) {
        $EditorArguments += "-PhysAnimProductRepetition=$ProductRepetition"
    }
    if ($SourceCommit) {
        $EditorArguments += "-PhysAnimSourceCommit=$SourceCommit"
    }
    if ($ModelOnnxSha256) {
        $EditorArguments += "-PhysAnimModelOnnxSha256=$ModelOnnxSha256"
    }
    if ($ActionSemanticTrace) {
        $EditorArguments += "-PhysAnimActionSemanticTrace"
    }
    if ($MannyLocalFrameRoundtripTrace) {
        $EditorArguments += "-PhysAnimMannyLocalFrameRoundtripTrace"
    }
    if ($ExperimentalComponentActionAxis) {
        $EditorArguments += "-PhysAnimExperimentalComponentActionAxis"
    }
    if ($ExperimentalComponentActionAxisFromFirstPolicy) {
        $EditorArguments += "-PhysAnimExperimentalComponentActionAxisFromFirstPolicy"
    }
    if ($ExperimentalBindNeutralFromFirstPolicy) {
        $EditorArguments += "-PhysAnimExperimentalBindNeutralFromFirstPolicy"
    }
    if ($ExperimentalConstraintRangeRemapBypassFromFirstPolicy) {
        $EditorArguments += "-PhysAnimExperimentalConstraintRangeRemapBypassFromFirstPolicy"
    }
    if ($ExperimentalActionFamily) {
        $EditorArguments += "-PhysAnimExperimentalActionFamily=$ExperimentalActionFamily"
    }
    if ($ExperimentalActionJointStart -ge 0) {
        $EditorArguments += "-PhysAnimExperimentalActionJointStart=$ExperimentalActionJointStart"
        $EditorArguments += "-PhysAnimExperimentalActionJointCount=$ExperimentalActionJointCount"
    }
    if ($ExperimentalPolicyActionBaselineResidual) {
        $EditorArguments += "-PhysAnimExperimentalPolicyActionBaselineResidual"
    }
    if ($ExperimentalPolicyActionZeroUntilBaseline) {
        $EditorArguments += "-PhysAnimExperimentalPolicyActionZeroUntilBaseline"
    }
    if ($ExperimentalPhysicsBodyObservationPositions) {
        $EditorArguments += "-PhysAnimExperimentalPhysicsBodyObservationPositions"
    }
    if ([Math]::Abs($ExperimentalActiveStrengthFactor - 1.0) -gt 1.0e-9) {
        $EditorArguments += "-PhysAnimExperimentalActiveStrengthFactor=$ExperimentalActiveStrengthFactor"
    }
    if ($ExperimentalCheckpointTorqueCeiling) {
        $EditorArguments += "-PhysAnimExperimentalCheckpointTorqueCeiling"
    }
    if ($ExperimentalCheckpointForcePd) {
        $EditorArguments += "-PhysAnimExperimentalCheckpointForcePd"
    }
    if ($PolicyInputProvenanceTrace) {
        $EditorArguments += "-PhysAnimPolicyInputProvenanceTrace"
    }
    if ($StartupChronologyTrace) {
        $EditorArguments += "-PhysAnimStartupChronologyTrace"
    }
    $EditorArguments += "-PhysAnimSourceTreeDirty=$(if ($SourceTreeDirty) { 1 } else { 0 })"

    & "$EditorCmdExe" $EditorArguments

    if ($LASTEXITCODE -ne 0) {
        Write-Host "!!! AUTOMATION TEST FAILED !!!" -ForegroundColor Red
        exit $LASTEXITCODE
    }
}

# 7. Summary & Launch
$Stopwatch.Stop()
$Time = $Stopwatch.Elapsed.ToString('mm\:ss')

if ($Run) {
    Write-Host "--- Build Succeeded ($Time)! Launching Editor... ---" -ForegroundColor Cyan
    Start-Process -FilePath "$EditorExe" -ArgumentList "`"$ProjectFile`""
}
else {
    Write-Host "--- Tasks Complete ($Time). ---" -ForegroundColor Yellow
}

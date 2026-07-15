param (
    [switch]$Full,  # Perform full plugin packaging (Slow)
    [switch]$Run,   # Launch the Editor on success
    [switch]$Clean,  # Wipe Binaries/Intermediate before building
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
    [switch]$PolicyInputProvenanceTrace,
    [switch]$StartupChronologyTrace
)

# 1. Environment Setup
if (-not $env:UE5_PATH) {
    Write-Host "Environment variables not found. Loading local paths..." -ForegroundColor Cyan
    . "$PSScriptRoot\local.paths.ps1"
}

$ProjectDir = "$PWD\PhysAnimUE5"
$ProjectFile = "$ProjectDir\PhysAnimUE5.uproject"
$PluginDir = "$ProjectDir\Plugins\PhysAnimPlugin"
$PluginFile = "$PluginDir\PhysAnimPlugin.uplugin"
$PackageDir = "$PWD\_build\PhysAnimPlugin"
$EditorExe = "$env:UE5_PATH\Binaries\Win64\UnrealEditor.exe"
$EditorCmdExe = "$env:UE5_PATH\Binaries\Win64\UnrealEditor-Cmd.exe"

$Stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

# 2. Kill Unreal-related tasks to prevent file locks
Write-Host "--- Checking for running Unreal processes ---" -ForegroundColor Yellow
$ProcessNames = @("UnrealEditor", "ShaderCompileWorker", "UnrealMultiUserServer", "CrashReportClient")

foreach ($Name in $ProcessNames) {
    $RunningProcs = Get-Process -Name $Name -ErrorAction SilentlyContinue
    if ($RunningProcs) {
        Write-Host "Closing $Name..." -ForegroundColor Magenta
        $RunningProcs | Stop-Process -Force
        Start-Sleep -Seconds 1 # Wait for OS to release file handles
    }
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

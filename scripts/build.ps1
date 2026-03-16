param (
    [switch]$Full,  # Perform full plugin packaging (Slow)
    [switch]$Run,   # Launch the Editor on success
    [switch]$Clean  # Wipe Binaries/Intermediate before building
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
Write-Host "[1] Compiling Editor Binaries..." -ForegroundColor Green
& "$env:UE5_PATH\Build\BatchFiles\Build.bat" PhysAnimUE5Editor Win64 Development `
    -Project="$ProjectFile" `
    -Progress -NoHotReloadFromIDE

if ($LASTEXITCODE -ne 0) { 
    Write-Host "!!! COMPILATION FAILED !!!" -ForegroundColor Red
    exit $LASTEXITCODE 
}

# 5. Full Packaging (Only if -Full is passed)
if ($Full) {
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

# 6. Summary & Launch
$Stopwatch.Stop()
$Time = $Stopwatch.Elapsed.ToString('mm\:ss')

if ($Run) {
    Write-Host "--- Build Succeeded ($Time)! Launching Editor... ---" -ForegroundColor Cyan
    Start-Process -FilePath "$EditorExe" -ArgumentList "`"$ProjectFile`""
}
else {
    Write-Host "--- Tasks Complete ($Time). ---" -ForegroundColor Yellow
}
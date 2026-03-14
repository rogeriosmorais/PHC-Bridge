param (
    [switch]$Test, # Fast iteration (Editor binaries only)
    [switch]$Run   # Launch the Editor on success
)

# 1. Ensure environment variables are loaded
if (-not $UE5_PATH) {
    Write-Host "Environment variables not found. Loading local paths..." -ForegroundColor Cyan
    . "$PSScriptRoot\local.paths.ps1"
}

# Paths
$ProjectFile = "$PWD\PhysAnimUE5\PhysAnimUE5.uproject"
$PluginFile = "$PWD\PhysAnimUE5\Plugins\PhysAnimPlugin\PhysAnimPlugin.uplugin"
$PackageDir = "$PWD\_build\PhysAnimPlugin"
$EditorExe = "$UE5_PATH\Binaries\Win64\UnrealEditor.exe"

Write-Host "--- Starting Build Process (Mode: $(if($Test){"Test"}else{"Full Package"})) ---" -ForegroundColor Yellow

# 2. Build Step
Write-Host "[1] Compiling Editor Binaries..." -ForegroundColor Green
& "$UE5_PATH\Build\BatchFiles\Build.bat" PhysAnimUE5Editor Win64 Development `
    -Project="$ProjectFile" `
    -Progress -NoHotReloadFromIDE

# Capture the success/fail of the compiler
$BuildResult = $LASTEXITCODE

if ($BuildResult -ne 0) { 
    Write-Host "!!! BUILD FAILED with exit code $BuildResult !!!" -ForegroundColor Red
    exit $BuildResult 
}

# 3. Conditional Packaging
if (-not $Test) {
    Write-Host "[2] Packaging Plugin..." -ForegroundColor Green
    & "$UE5_PATH\Build\BatchFiles\RunUAT.bat" BuildPlugin `
        -Plugin="$PluginFile" `
        -Package="$PackageDir" `
        -TargetPlatforms=Win64 `
        -Rocket -Force
}

# 4. Conditional Launch
if ($Run) {
    Write-Host "--- Build Succeeded! Launching Unreal Editor... ---" -ForegroundColor Cyan
    # Start-Process allows the script to finish while the Editor stays open
    Start-Process -FilePath "$EditorExe" -ArgumentList "`"$ProjectFile`""
}
else {
    Write-Host "--- Tasks Complete. ---" -ForegroundColor Yellow
}
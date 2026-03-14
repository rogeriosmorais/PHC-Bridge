param (
    [switch]$Test  # Add -Test to skip the packaging and shipping builds
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

Write-Host "--- Starting Build Process (Mode: $(if($Test){"Test/Iteration"}else{"Full Package"})) ---" -ForegroundColor Yellow

# 2. Build the Project and Plugin (Editor only)
Write-Host "[1] Compiling Editor Binaries..." -ForegroundColor Green
& "$UE5_PATH\Build\BatchFiles\Build.bat" PhysAnimUE5Editor Win64 Development `
    -Project="$ProjectFile" `
    -Progress -NoHotReloadFromIDE

if ($LASTEXITCODE -ne 0) { 
    Write-Error "Build Failed!"; exit $LASTEXITCODE 
}

# 3. Conditional Packaging (Skip if -Test is present)
if ($Test) {
    Write-Host "--- Test Build Complete. Skipping Packaging Step. ---" -ForegroundColor Cyan
}
else {
    Write-Host "[2] Packaging Plugin (Game + Shipping builds)..." -ForegroundColor Green
    & "$UE5_PATH\Build\BatchFiles\RunUAT.bat" BuildPlugin `
        -Plugin="$PluginFile" `
        -Package="$PackageDir" `
        -TargetPlatforms=Win64 `
        -Rocket -Force
    
    Write-Host "--- Full Build and Package Complete! ---" -ForegroundColor Cyan
}
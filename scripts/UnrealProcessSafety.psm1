Set-StrictMode -Version Latest

function Resolve-CanonicalDirectoryPath {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "Path must not be empty."
    }

    $FullPath = [System.IO.Path]::GetFullPath($Path)
    return $FullPath.TrimEnd(
        [char[]]@(
            [System.IO.Path]::DirectorySeparatorChar,
            [System.IO.Path]::AltDirectorySeparatorChar
        )
    )
}

function Get-UnrealEngineBuildVersion {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$EngineRoot
    )

    $CanonicalRoot = Resolve-CanonicalDirectoryPath -Path $EngineRoot
    $BuildVersionPath = Join-Path $CanonicalRoot "Build\Build.version"
    if (-not (Test-Path -LiteralPath $BuildVersionPath -PathType Leaf)) {
        throw "Unreal Engine Build.version is missing: $BuildVersionPath"
    }

    try {
        $Version = Get-Content -Raw -LiteralPath $BuildVersionPath | ConvertFrom-Json
    }
    catch {
        throw "Unreal Engine Build.version is invalid: $BuildVersionPath. $($_.Exception.Message)"
    }

    foreach ($Field in @("MajorVersion", "MinorVersion")) {
        if ($null -eq $Version.$Field) {
            throw "Unreal Engine Build.version is missing ${Field}: $BuildVersionPath"
        }
    }

    return [pscustomobject]@{
        EngineRoot = $CanonicalRoot
        MajorVersion = [int]$Version.MajorVersion
        MinorVersion = [int]$Version.MinorVersion
        PatchVersion = if ($null -ne $Version.PatchVersion) { [int]$Version.PatchVersion } else { 0 }
        BuildVersionPath = $BuildVersionPath
    }
}

function Assert-UnrealEngineVersion {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$EngineRoot,

        [int]$ExpectedMajorVersion = 5,
        [int]$ExpectedMinorVersion = 7
    )

    $Version = Get-UnrealEngineBuildVersion -EngineRoot $EngineRoot
    if (
        $Version.MajorVersion -ne $ExpectedMajorVersion -or
        $Version.MinorVersion -ne $ExpectedMinorVersion
    ) {
        throw (
            "Refusing to continue: expected Unreal Engine " +
            "$ExpectedMajorVersion.$ExpectedMinorVersion, but UE5_PATH points to " +
            "$($Version.MajorVersion).$($Version.MinorVersion) at $($Version.EngineRoot)."
        )
    }

    return $Version
}

function Test-PathIsWithinRoot {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$CandidatePath,

        [Parameter(Mandatory = $true)]
        [string]$RootPath
    )

    $CanonicalCandidate = [System.IO.Path]::GetFullPath($CandidatePath)
    $CanonicalRoot = Resolve-CanonicalDirectoryPath -Path $RootPath
    if ($CanonicalCandidate.Equals($CanonicalRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $true
    }

    $RootPrefix = $CanonicalRoot + [System.IO.Path]::DirectorySeparatorChar
    return $CanonicalCandidate.StartsWith(
        $RootPrefix,
        [System.StringComparison]::OrdinalIgnoreCase
    )
}

function Get-EngineOwnedUnrealProcesses {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$EngineRoot,

        [string[]]$ProcessNames = @(
            "UnrealEditor",
            "UnrealEditor-Cmd",
            "ShaderCompileWorker",
            "UnrealMultiUserServer",
            "CrashReportClient"
        ),

        [scriptblock]$ProcessProvider = {
            param($Names)
            Get-Process -Name $Names -ErrorAction SilentlyContinue
        }
    )

    $CanonicalRoot = Resolve-CanonicalDirectoryPath -Path $EngineRoot
    $Processes = @(& $ProcessProvider $ProcessNames)
    foreach ($Process in $Processes) {
        $ExecutablePath = $null
        try {
            $ExecutablePath = [string]$Process.Path
        }
        catch {
            $ExecutablePath = $null
        }

        # Fail closed: a process with an inaccessible or missing executable path is never owned.
        if ([string]::IsNullOrWhiteSpace($ExecutablePath)) {
            continue
        }

        if (Test-PathIsWithinRoot -CandidatePath $ExecutablePath -RootPath $CanonicalRoot) {
            Write-Output $Process
        }
    }
}

function Stop-EngineOwnedUnrealProcesses {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$EngineRoot,

        [int]$ExpectedMajorVersion = 5,
        [int]$ExpectedMinorVersion = 7,

        [string[]]$ProcessNames = @(
            "UnrealEditor",
            "UnrealEditor-Cmd",
            "ShaderCompileWorker",
            "UnrealMultiUserServer",
            "CrashReportClient"
        ),

        [scriptblock]$ProcessProvider = {
            param($Names)
            Get-Process -Name $Names -ErrorAction SilentlyContinue
        },

        [scriptblock]$StopProcessAction = {
            param($Process)
            $Process | Stop-Process -Force -ErrorAction Stop
        }
    )

    $null = Assert-UnrealEngineVersion `
        -EngineRoot $EngineRoot `
        -ExpectedMajorVersion $ExpectedMajorVersion `
        -ExpectedMinorVersion $ExpectedMinorVersion

    $OwnedProcesses = @(
        Get-EngineOwnedUnrealProcesses `
            -EngineRoot $EngineRoot `
            -ProcessNames $ProcessNames `
            -ProcessProvider $ProcessProvider
    )

    foreach ($Process in $OwnedProcesses) {
        $null = & $StopProcessAction $Process
        Write-Output $Process
    }
}

Export-ModuleMember -Function @(
    "Get-UnrealEngineBuildVersion",
    "Assert-UnrealEngineVersion",
    "Test-PathIsWithinRoot",
    "Get-EngineOwnedUnrealProcesses",
    "Stop-EngineOwnedUnrealProcesses"
)

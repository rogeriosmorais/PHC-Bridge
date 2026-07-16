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

function Stop-ValidatedUnrealProcessTree {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [int]$RootProcessId,

        [Parameter(Mandatory = $true)]
        [string]$EngineRoot,

        [int]$ExpectedMajorVersion = 5,
        [int]$ExpectedMinorVersion = 7,

        [scriptblock]$ProcessSnapshotProvider = {
            Get-CimInstance Win32_Process |
                Select-Object ProcessId, ParentProcessId, Name, ExecutablePath
        },

        [scriptblock]$StopProcessAction = {
            param($ProcessRecord)
            Stop-Process -Id ([int]$ProcessRecord.ProcessId) -Force -ErrorAction SilentlyContinue
        }
    )

    $null = Assert-UnrealEngineVersion `
        -EngineRoot $EngineRoot `
        -ExpectedMajorVersion $ExpectedMajorVersion `
        -ExpectedMinorVersion $ExpectedMinorVersion

    $CanonicalRoot = Resolve-CanonicalDirectoryPath -Path $EngineRoot
    $Snapshot = @(& $ProcessSnapshotProvider)
    $ById = @{}
    foreach ($Record in $Snapshot) {
        $ById[[int]$Record.ProcessId] = $Record
    }

    if (-not $ById.ContainsKey($RootProcessId)) {
        return
    }

    $Selected = [System.Collections.Generic.List[object]]::new()
    $DepthById = @{}
    $Queue = [System.Collections.Generic.Queue[object]]::new()
    $Queue.Enqueue([pscustomobject]@{ ProcessId = $RootProcessId; Depth = 0 })
    while ($Queue.Count -gt 0) {
        $Current = $Queue.Dequeue()
        if (-not $ById.ContainsKey([int]$Current.ProcessId)) {
            continue
        }

        $Record = $ById[[int]$Current.ProcessId]
        $Selected.Add($Record)
        $DepthById[[int]$Record.ProcessId] = [int]$Current.Depth
        foreach ($Child in $Snapshot) {
            if ([int]$Child.ParentProcessId -eq [int]$Record.ProcessId) {
                $Queue.Enqueue([pscustomobject]@{
                    ProcessId = [int]$Child.ProcessId
                    Depth = [int]$Current.Depth + 1
                })
            }
        }
    }

    $UnrealNames = @(
        "UnrealEditor.exe",
        "UnrealEditor-Cmd.exe",
        "ShaderCompileWorker.exe",
        "UnrealMultiUserServer.exe",
        "CrashReportClient.exe"
    )
    foreach ($Record in $Selected) {
        if ($UnrealNames -contains [string]$Record.Name) {
            $ExecutablePath = [string]$Record.ExecutablePath
            if (
                [string]::IsNullOrWhiteSpace($ExecutablePath) -or
                -not (Test-PathIsWithinRoot -CandidatePath $ExecutablePath -RootPath $CanonicalRoot)
            ) {
                throw (
                    "Refusing timeout cleanup: process $($Record.ProcessId) " +
                    "$($Record.Name) is not proven to belong to UE " +
                    "$ExpectedMajorVersion.$ExpectedMinorVersion at $CanonicalRoot."
                )
            }
        }
    }

    $Ordered = @($Selected | Sort-Object {
        -1 * [int]$DepthById[[int]$_.ProcessId]
    })
    foreach ($Record in $Ordered) {
        $null = & $StopProcessAction $Record
        Write-Output $Record
    }
}

Export-ModuleMember -Function @(
    "Get-UnrealEngineBuildVersion",
    "Assert-UnrealEngineVersion",
    "Test-PathIsWithinRoot",
    "Get-EngineOwnedUnrealProcesses",
    "Stop-EngineOwnedUnrealProcesses",
    "Stop-ValidatedUnrealProcessTree"
)

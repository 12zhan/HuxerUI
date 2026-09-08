[CmdletBinding()]
param(
    [string]$Version,
    [string]$Prefix,
    [string]$Archive,
    [switch]$Yes,
    [switch]$Uninstall,
    [switch]$Update,
    [switch]$Check,
    [int]$WaitForCli,
    [int]$WaitProcessId
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepositoryUrl = "https://github.com/HuxerUI/HuxerUI"
$ExplicitVersion = [bool]$Version

if ($WaitProcessId) {
    Start-Transcript -Path (Join-Path $PSScriptRoot "update.log") -Force | Out-Null
    $ParentProcess = Get-Process -Id $WaitProcessId -ErrorAction SilentlyContinue
    if ($ParentProcess -and -not $ParentProcess.WaitForExit(60000)) {
        throw "HuxerUI updater timed out waiting for the original CLI to exit"
    }
}

function Fail([string]$Message) {
    throw "HuxerUI installer: $Message"
}

function Get-AbsolutePath([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location).Path $Path))
}

function Test-HuxerUISdk([string]$Root) {
    return (Test-Path -LiteralPath (Join-Path $Root "bin/huxerui.exe") -PathType Leaf) -and
        (Test-Path -LiteralPath (Join-Path $Root "include/huxerui/huxerui.h") -PathType Leaf) -and
        (Test-Path -LiteralPath (Join-Path $Root "lib/cmake/HuxerUI/HuxerUIConfig.cmake") -PathType Leaf) -and
        (Test-Path -LiteralPath (Join-Path $Root "share/huxerui/resources/huxerui/resources.bin") -PathType Leaf)
}

function Get-SdkVersion([string]$Root) {
    $PreviousHome = $env:HUXERUI_HOME
    try {
        $env:HUXERUI_HOME = $Root
        $Result = & (Join-Path $Root "bin/huxerui.exe") --version
        if ($LASTEXITCODE -ne 0 -or $Result -notmatch '^huxerui ([0-9]+\.[0-9]+\.[0-9]+)$') {
            Fail "SDK CLI cannot report its release version: $Root"
        }
        return $Matches[1]
    } finally {
        $env:HUXERUI_HOME = $PreviousHome
    }
}

function Lock-Sdk {
    if ((Test-Path -LiteralPath $Prefix) -and
        ((Get-Item -LiteralPath $Prefix).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        Fail "installation prefix must not be a link: $Prefix"
    }
    try {
        return [IO.File]::Open("$Prefix.huxerui-lock", [IO.FileMode]::CreateNew, [IO.FileAccess]::Write,
            [IO.FileShare]::None)
    } catch {
        Fail "another installer may be using this SDK; inspect $Prefix.huxerui-lock before retrying"
    }
}

function Confirm-Action([string]$Description) {
    Write-Host $Description
    if ($Yes) {
        return
    }
    $Answer = Read-Host "Continue? [y/N]"
    if ($Answer -notmatch "^(?i:y|yes)$") {
        Fail "cancelled"
    }
}

function Assert-SafePrefix([string]$Path) {
    $Root = [System.IO.Path]::GetPathRoot($Path)
    if ([string]::IsNullOrWhiteSpace($Path) -or
        $Path.Equals($Root, [System.StringComparison]::OrdinalIgnoreCase) -or
        $Path.Equals($env:USERPROFILE, [System.StringComparison]::OrdinalIgnoreCase)) {
        Fail "unsafe installation prefix: $Path"
    }
}

function Set-UserEnvironment([string]$SdkRoot) {
    $OldHome = [Environment]::GetEnvironmentVariable("HUXERUI_HOME", "User")
    $OldPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $Entries = [System.Collections.Generic.List[string]]::new()
    if ($OldPath) {
        foreach ($Entry in $OldPath.Split(";", [System.StringSplitOptions]::RemoveEmptyEntries)) {
            if ($OldHome -and $Entry.Equals((Join-Path $OldHome "bin"), [System.StringComparison]::OrdinalIgnoreCase)) {
                continue
            }
            if (-not $Entry.Equals((Join-Path $SdkRoot "bin"), [System.StringComparison]::OrdinalIgnoreCase)) {
                $Entries.Add($Entry)
            }
        }
    }
    $Entries.Add((Join-Path $SdkRoot "bin"))
    try {
        [Environment]::SetEnvironmentVariable("HUXERUI_HOME", $SdkRoot, "User")
        [Environment]::SetEnvironmentVariable("Path", ($Entries -join ";"), "User")
    } catch {
        [Environment]::SetEnvironmentVariable("HUXERUI_HOME", $OldHome, "User")
        [Environment]::SetEnvironmentVariable("Path", $OldPath, "User")
        throw
    }
}

function Remove-UserEnvironment([string]$SdkRoot) {
    $CurrentHome = [Environment]::GetEnvironmentVariable("HUXERUI_HOME", "User")
    if (-not $CurrentHome -or
        -not $CurrentHome.Equals($SdkRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        return
    }
    $CurrentPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $Entries = [System.Collections.Generic.List[string]]::new()
    if ($CurrentPath) {
        foreach ($Entry in $CurrentPath.Split(";", [System.StringSplitOptions]::RemoveEmptyEntries)) {
            if (-not $Entry.Equals((Join-Path $SdkRoot "bin"), [System.StringComparison]::OrdinalIgnoreCase)) {
                $Entries.Add($Entry)
            }
        }
    }
    [Environment]::SetEnvironmentVariable("HUXERUI_HOME", $null, "User")
    [Environment]::SetEnvironmentVariable("Path", ($Entries -join ";"), "User")
}

if ($Check -and -not $Update) {
    Fail "-Check requires -Update"
}
if ($Update -and (-not $Prefix -or $Uninstall)) {
    Fail "-Update requires -Prefix and cannot be combined with -Uninstall"
}
if (($WaitForCli -or $WaitProcessId) -and (-not $Update -or $Archive)) {
    Fail "CLI handoff requires -Update without -Archive"
}
if ($Uninstall -and ($Version -or $Archive)) {
    Fail "-Uninstall cannot be combined with -Version or -Archive"
}
if ($Version -and $Archive) {
    Fail "-Version and -Archive cannot be combined"
}
if (-not $env:LOCALAPPDATA) {
    Fail "LOCALAPPDATA is not configured"
}
if (-not $Prefix) {
    $Prefix = Join-Path $env:LOCALAPPDATA "HuxerUI"
}
$Prefix = Get-AbsolutePath $Prefix
Assert-SafePrefix $Prefix

if ($Uninstall) {
    if (-not (Test-HuxerUISdk $Prefix)) {
        Fail "refusing to remove a directory that is not a HuxerUI SDK: $Prefix"
    }
    Confirm-Action "Uninstall HuxerUI SDK`n  SDK: $Prefix"
    $SdkLock = Lock-Sdk
    try {
        Remove-UserEnvironment $Prefix
        Remove-Item -LiteralPath $Prefix -Recurse -Force
    } finally {
        $SdkLock.Dispose()
        Remove-Item -LiteralPath "$Prefix.huxerui-lock"
    }
    Write-Host "HuxerUI SDK removed from $Prefix"
    exit 0
}

$Architecture = $(if ($env:PROCESSOR_ARCHITEW6432) { $env:PROCESSOR_ARCHITEW6432 } else { $env:PROCESSOR_ARCHITECTURE }).ToLowerInvariant()

if ($Architecture -notin @("amd64", "x64", "x86_64")) {
    Fail "Windows SDK archives currently support x86_64 hosts only"
}

$HostArchitecture = "x86_64"

if ($Update) {
    if (-not (Test-HuxerUISdk $Prefix)) {
        Fail "-Update requires an installed HuxerUI SDK: $Prefix"
    }
    if (Test-Path -LiteralPath (Join-Path $Prefix "CMakeLists.txt")) {
        Fail "refusing to update a source checkout"
    }
    $CurrentVersion = Get-SdkVersion $Prefix
}

$ReleaseTag = $null
if (-not $Archive) {
    if (-not $Version) {
        $ApiUrl = $RepositoryUrl -replace '^https://github\.com/', 'https://api.github.com/repos/'
        
        try {
            $Release = Invoke-RestMethod -Uri "$ApiUrl/releases/latest" -Headers @{
                Accept       = "application/vnd.github+json"
                "User-Agent" = "PowerShell-Installer"
            }
        } catch {
            Fail "Failed to query latest release from API: $_"
        }

        $ReleaseTag = [string]$Release.tag_name
        if (-not $ReleaseTag) {
            Fail "GitHub API did not return a valid release. Response: $($Release | ConvertTo-Json -Depth 2)"
        }
        if (-not $ReleaseTag.StartsWith("v")) {
            Fail "latest HuxerUI release has an invalid tag: $ReleaseTag"
        }
        $Version = $ReleaseTag.Substring(1)
    } else {
        $Version = $Version.TrimStart("v")
        $ReleaseTag = "v$Version"
    }
    if ($Version -notmatch "^[0-9A-Za-z._-]+$") {
        Fail "invalid version: $Version"
    }
    
    $ArchiveName = "huxerui-sdk-$Version-windows-$HostArchitecture.zip"
    $ArchiveSource = "$RepositoryUrl/releases/download/$ReleaseTag/$ArchiveName"
    $ArchiveDisplay = $ArchiveSource
} else {
    $Archive = Get-AbsolutePath $Archive
    if (-not (Test-Path -LiteralPath $Archive -PathType Leaf)) {
        Fail "archive does not exist: $Archive"
    }
    $ArchiveName = Split-Path -Leaf $Archive
    $ArchiveDisplay = $Archive
}

if ($ArchiveName -notmatch "^huxerui-sdk-.+-windows-x86_64\.zip$") {
    Fail "archive does not match this host: $ArchiveName"
}
if ((Test-Path -LiteralPath $Prefix) -and -not (Test-HuxerUISdk $Prefix)) {
    Fail "installation prefix exists but is not a HuxerUI SDK: $Prefix"
}

if ($Update) {
    if ($ArchiveName -notmatch '^huxerui-sdk-([0-9]+\.[0-9]+\.[0-9]+)-windows-x86_64\.zip$') {
        Fail "expected a major.minor.patch release version"
    }
    $Version = $Matches[1]
    Write-Host "Update HuxerUI SDK`n  SDK: $Prefix`n  Current: $CurrentVersion`n  Target: $Version`n  Package: windows-x86_64"
    if ([version]$CurrentVersion -eq [version]$Version) {
        Write-Host "HuxerUI SDK is already at the requested version."
        exit 0
    }
    if (-not $ExplicitVersion -and -not $Archive -and [version]$CurrentVersion -gt [version]$Version) {
        Write-Host "The installed SDK is newer; use --version to explicitly downgrade."
        exit 0
    }
    if ($Check) {
        Write-Host "HuxerUI SDK update available."
        exit 0
    }
    Confirm-Action "The entire SDK will be replaced, including local modifications. Stop SDK builds and tools first."
    if ($WaitForCli) {
        $QuotedScript = $PSCommandPath.Replace("'", "''")
        $QuotedPrefix = $Prefix.Replace("'", "''")
        $Worker = "& '$QuotedScript' -Update -Prefix '$QuotedPrefix' -Version '$Version' -Yes -WaitProcessId $WaitForCli"
        $Encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($Worker))
        Start-Process -FilePath (Join-Path $PSHOME "powershell.exe") -ArgumentList @(
            "-NoProfile", "-ExecutionPolicy", "Bypass", "-EncodedCommand", $Encoded
        ) -NoNewWindow | Out-Null
        exit 10
    }
} else {
    Confirm-Action "Install HuxerUI SDK`n  Archive: $ArchiveDisplay`n  SDK: $Prefix"
}

$TemporaryDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ("huxerui-sdk-" + [guid]::NewGuid())
$StagingDirectory = $null
$BackupDirectory = $null
$SdkLock = $null
New-Item -ItemType Directory -Path $TemporaryDirectory | Out-Null
try {
    if ($Update) {
        $SdkLock = Lock-Sdk
        if ((Get-SdkVersion $Prefix) -ne $CurrentVersion) {
            Fail "SDK changed during the update check; retry"
        }
    }
    if (-not $Archive) {
        $Archive = Join-Path $TemporaryDirectory $ArchiveName
        Invoke-WebRequest -Uri $ArchiveSource -OutFile $Archive -UseBasicParsing
        Invoke-WebRequest -Uri "$ArchiveSource.sha256" -OutFile "$Archive.sha256" -UseBasicParsing
    }

    $ChecksumPath = "$Archive.sha256"
    if (-not (Test-Path -LiteralPath $ChecksumPath -PathType Leaf)) {
        Fail "archive checksum does not exist: $ChecksumPath"
    }
    $ChecksumParts = ((Get-Content -LiteralPath $ChecksumPath -TotalCount 1).Trim() -split "\s+")
    if ($ChecksumParts.Count -lt 2 -or $ChecksumParts[1] -ne $ArchiveName) {
        Fail "archive checksum is malformed or names a different file: $ChecksumPath"
    }
    $ActualChecksum = (Get-FileHash -LiteralPath $Archive -Algorithm SHA256).Hash
    if (-not $ActualChecksum.Equals($ChecksumParts[0], [System.StringComparison]::OrdinalIgnoreCase)) {
        Fail "archive checksum does not match: $ArchiveName"
    }

    $ExtractDirectory = Join-Path $TemporaryDirectory "extract"
    $ArchiveRoot = $ArchiveName.Substring(0, $ArchiveName.Length - ".zip".Length)
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $Zip = [IO.Compression.ZipFile]::OpenRead($Archive)
    try {
        foreach ($Entry in $Zip.Entries) {
            $Name = $Entry.FullName.Replace('\', '/')
            if (-not $Name.StartsWith("$ArchiveRoot/", [StringComparison]::Ordinal) -or
                $Name -match '(^|/)\.\.(/|$)|:' -or
                (($Entry.ExternalAttributes -shr 16) -band 0xF000) -eq 0xA000) {
                Fail "archive contains an invalid path or symbolic link"
            }
        }
    } finally {
        $Zip.Dispose()
    }
    Expand-Archive -LiteralPath $Archive -DestinationPath $ExtractDirectory
    $ExtractedSdk = Join-Path $ExtractDirectory $ArchiveRoot
    if (-not (Test-HuxerUISdk $ExtractedSdk)) {
        Fail "archive does not contain a complete HuxerUI SDK"
    }
    if ($Update -and (Get-SdkVersion $ExtractedSdk) -ne $Version) {
        Fail "archive SDK version does not match $Version"
    }

    $Parent = Split-Path -Parent $Prefix
    New-Item -ItemType Directory -Path $Parent -Force | Out-Null
    if (-not $SdkLock) {
        $SdkLock = Lock-Sdk
    }
    $StagingDirectory = Join-Path $Parent (".huxerui-install-" + [guid]::NewGuid())
    Move-Item -LiteralPath $ExtractedSdk -Destination $StagingDirectory
    if (Test-Path -LiteralPath $Prefix) {
        $BackupDirectory = Join-Path $Parent (".huxerui-backup-" + [guid]::NewGuid())
        Move-Item -LiteralPath $Prefix -Destination $BackupDirectory
    }
    try {
        Move-Item -LiteralPath $StagingDirectory -Destination $Prefix
        $StagingDirectory = $null
        if ($Update) {
            if ((Get-SdkVersion $Prefix) -ne $Version) {
                Fail "installed SDK version does not match $Version"
            }
        } else {
            Set-UserEnvironment $Prefix
        }
    } catch {
        Remove-Item -LiteralPath $Prefix -Recurse -Force -ErrorAction SilentlyContinue
        if ($BackupDirectory -and (Test-Path -LiteralPath $BackupDirectory)) {
            Move-Item -LiteralPath $BackupDirectory -Destination $Prefix
            $BackupDirectory = $null
        }
        throw
    }

    if ($BackupDirectory -and (Test-Path -LiteralPath $BackupDirectory)) {
        Remove-Item -LiteralPath $BackupDirectory -Recurse -Force
        $BackupDirectory = $null
    }
} finally {
    if ($SdkLock) {
        $SdkLock.Dispose()
        Remove-Item -LiteralPath "$Prefix.huxerui-lock"
    }
    if ($StagingDirectory -and (Test-Path -LiteralPath $StagingDirectory)) {
        Remove-Item -LiteralPath $StagingDirectory -Recurse -Force
    }
    if ($BackupDirectory -and (Test-Path -LiteralPath $BackupDirectory)) {
        Write-Warning "Previous HuxerUI SDK remains at $BackupDirectory"
    }
    if (Test-Path -LiteralPath $TemporaryDirectory) {
        Remove-Item -LiteralPath $TemporaryDirectory -Recurse -Force
    }
}

Write-Host "HuxerUI SDK installed at $Prefix"
if (-not $Update) {
    Write-Host "Restart the terminal to use the updated user environment."
}
if ($WaitProcessId) {
    Stop-Transcript | Out-Null
}

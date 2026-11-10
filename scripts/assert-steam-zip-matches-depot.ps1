#requires -Version 5.1

[CmdletBinding(PositionalBinding = $false)]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$ZipPath,

    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$DepotRoot,
    [ValidateSet('release_candidate','engineering_candidate')][string]$ExpectedPackageKind = 'release_candidate'
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Get-NormalizedFullPath {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$BasePath
    )

    $candidate = $Path
    if (-not [System.IO.Path]::IsPathRooted($candidate)) {
        $candidate = Join-Path $BasePath $candidate
    }
    return [System.IO.Path]::GetFullPath($candidate).TrimEnd('\', '/')
}

function Test-IsSameOrChildPath {
    param(
        [Parameter(Mandatory = $true)][string]$Child,
        [Parameter(Mandatory = $true)][string]$Parent
    )

    $normalizedChild = [System.IO.Path]::GetFullPath($Child).TrimEnd('\', '/')
    $normalizedParent = [System.IO.Path]::GetFullPath($Parent).TrimEnd('\', '/')
    if ($normalizedChild.Equals($normalizedParent, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $true
    }
    return $normalizedChild.StartsWith(
        $normalizedParent + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase)
}

function ConvertTo-SafeArchivePath {
    param(
        [Parameter(Mandatory = $true)][string]$EntryPath,
        [Parameter(Mandatory = $true)][bool]$IsDirectory
    )

    $normalized = $EntryPath.Replace('\', '/')
    if ($IsDirectory) {
        if (-not $normalized.EndsWith('/', [System.StringComparison]::Ordinal)) {
            throw "ZIP directory entry does not end with '/': $EntryPath"
        }
        $normalized = $normalized.Substring(0, $normalized.Length - 1)
    }
    elseif ($normalized.EndsWith('/', [System.StringComparison]::Ordinal)) {
        throw "ZIP file entry unexpectedly ends with '/': $EntryPath"
    }

    if ([string]::IsNullOrEmpty($normalized) -or
        $normalized.StartsWith('/', [System.StringComparison]::Ordinal) -or
        [System.IO.Path]::IsPathRooted($normalized) -or
        $normalized -match '^[A-Za-z]:') {
        throw "ZIP contains a rooted or empty entry path: $EntryPath"
    }

    $segments = $normalized.Split(
        [char[]]@('/'),
        [System.StringSplitOptions]::None)
    $invalidFileNameCharacters = [System.IO.Path]::GetInvalidFileNameChars()
    foreach ($segment in $segments) {
        if ([string]::IsNullOrEmpty($segment) -or
            $segment -eq '.' -or
            $segment -eq '..' -or
            $segment.EndsWith('.', [System.StringComparison]::Ordinal) -or
            $segment.EndsWith(' ', [System.StringComparison]::Ordinal) -or
            $segment.IndexOfAny($invalidFileNameCharacters) -ge 0) {
            throw "ZIP contains an unsafe or non-canonical entry path: $EntryPath"
        }
    }

    return $normalized
}

function Get-StreamSha256 {
    param([Parameter(Mandatory = $true)][System.IO.Stream]$Stream)

    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $hashBytes = $sha256.ComputeHash($Stream)
        return ([System.BitConverter]::ToString($hashBytes)).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $sha256.Dispose()
    }
}

function Get-FileSha256 {
    param([Parameter(Mandatory = $true)][string]$Path)

    # Deny concurrent writers while the file is hashed. This prevents a size/hash
    # pair assembled from two different file states during this check.
    $stream = [System.IO.File]::Open(
        $Path,
        [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::Read)
    try {
        return Get-StreamSha256 -Stream $stream
    }
    finally {
        $stream.Dispose()
    }
}

function Throw-ComparisonFailure {
    param(
        [Parameter(Mandatory = $true)][string]$Stage,
        [Parameter(Mandatory = $true)][System.Collections.IEnumerable]$Errors
    )

    $allErrors = @($Errors)
    $visibleErrors = @($allErrors | Select-Object -First 20)
    $message = "ZIP/depot $Stage comparison failed ($($allErrors.Count) difference(s)):`n - " +
        ($visibleErrors -join "`n - ")
    if ($allErrors.Count -gt $visibleErrors.Count) {
        $message += "`n - ... and $($allErrors.Count - $visibleErrors.Count) more difference(s)"
    }
    throw $message
}

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..')).TrimEnd('\', '/')
$resolvedZipPath = Get-NormalizedFullPath -Path $ZipPath -BasePath $projectRoot
$resolvedDepotRoot = Get-NormalizedFullPath -Path $DepotRoot -BasePath $projectRoot

if (-not (Test-Path -LiteralPath $resolvedZipPath -PathType Leaf)) {
    throw "Steam ZIP does not exist: $resolvedZipPath"
}
if (-not [System.IO.Path]::GetExtension($resolvedZipPath).Equals(
        '.zip',
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Expected a .zip archive: $resolvedZipPath"
}
if (-not (Test-Path -LiteralPath $resolvedDepotRoot -PathType Container)) {
    throw "Steam depot root does not exist: $resolvedDepotRoot"
}
if (Test-IsSameOrChildPath -Child $resolvedZipPath -Parent $resolvedDepotRoot) {
    throw "ZIP must be outside the depot it is compared against: $resolvedZipPath"
}

$versionPath = Join-Path $resolvedDepotRoot 'VERSION.txt'
$draftNoticePath = Join-Path $resolvedDepotRoot 'Documentation/THIRD_PARTY_NOTICES_DRAFT.md'
if (-not (Test-Path -LiteralPath $versionPath -PathType Leaf)) {
    throw "Steam depot is missing its release-candidate marker: VERSION.txt"
}
if (-not (Test-Path -LiteralPath $draftNoticePath -PathType Leaf)) {
    throw "Steam depot is missing its draft/no-go notice: Documentation/THIRD_PARTY_NOTICES_DRAFT.md"
}
$packageKindLine = @(Get-Content -LiteralPath $versionPath | Where-Object {
        $_ -like 'package_kind=*'
    })
if ($packageKindLine.Count -ne 1 -or $packageKindLine[0] -ne "package_kind=$ExpectedPackageKind") {
    throw "VERSION.txt must contain exactly one 'package_kind=$ExpectedPackageKind' marker."
}
$draftNoticeText = Get-Content -Raw -LiteralPath $draftNoticePath
if ($draftNoticeText -notmatch '(?i)DRAFT' -or
    $draftNoticeText -notmatch '(?i)NOT (?:LEGAL OR )?DISTRIBUTION CLEARANCE') {
    throw "Third-party notice no longer contains the required draft/distribution no-go marker."
}

$depotItems = @(Get-ChildItem -LiteralPath $resolvedDepotRoot -Force -Recurse)
$reparsePoints = @($depotItems | Where-Object {
        ($_.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0
    })
if ($reparsePoints.Count -gt 0) {
    throw "Steam depot contains a reparse point, which cannot be compared as an ordinary ZIP entry: $($reparsePoints[0].FullName)"
}

$depotFiles = [System.Collections.Generic.Dictionary[string, object]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
$depotDirectories = [System.Collections.Generic.Dictionary[string, string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
foreach ($item in $depotItems) {
    $relativePath = $item.FullName.Substring($resolvedDepotRoot.Length).TrimStart('\', '/').Replace('\', '/')
    if ($item.PSIsContainer) {
        if ($depotDirectories.ContainsKey($relativePath)) {
            throw "Depot contains case-colliding directory paths: $relativePath"
        }
        $depotDirectories[$relativePath] = $relativePath
        continue
    }
    if ($depotFiles.ContainsKey($relativePath)) {
        throw "Depot contains case-colliding file paths: $relativePath"
    }
    $depotFiles[$relativePath] = [pscustomobject]@{
        Path = $relativePath
        FullName = $item.FullName
        Length = [long]$item.Length
    }
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [System.IO.Compression.ZipFile]::OpenRead($resolvedZipPath)
try {
    $archiveFiles = [System.Collections.Generic.Dictionary[string, object]]::new(
        [System.StringComparer]::OrdinalIgnoreCase)
    $archivePaths = [System.Collections.Generic.Dictionary[string, string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase)
    $directoryErrors = New-Object System.Collections.Generic.List[string]

    foreach ($entry in $archive.Entries) {
        $isDirectory = [string]::IsNullOrEmpty($entry.Name)
        $relativePath = ConvertTo-SafeArchivePath -EntryPath $entry.FullName -IsDirectory $isDirectory

        if ($archivePaths.ContainsKey($relativePath)) {
            throw "ZIP contains duplicate or case-colliding entry paths: '$($archivePaths[$relativePath])' and '$($entry.FullName)'"
        }
        $archivePaths[$relativePath] = $entry.FullName

        $unixFileType = (([uint32]$entry.ExternalAttributes -shr 16) -band 0xF000)
        if ($unixFileType -eq 0xA000) {
            throw "ZIP contains a symbolic-link entry: $($entry.FullName)"
        }

        if ($isDirectory) {
            if (-not $depotDirectories.ContainsKey($relativePath)) {
                $directoryErrors.Add("archive-only directory '$relativePath/'")
            }
            elseif (-not $relativePath.Equals(
                    $depotDirectories[$relativePath],
                    [System.StringComparison]::Ordinal)) {
                $directoryErrors.Add(
                    "directory path casing '$relativePath/' != '$($depotDirectories[$relativePath])/'")
            }
            continue
        }

        $archiveFiles[$relativePath] = [pscustomobject]@{
            Path = $relativePath
            Entry = $entry
            Length = [long]$entry.Length
        }
    }

    if ($directoryErrors.Count -gt 0) {
        Throw-ComparisonFailure -Stage 'directory-list' -Errors $directoryErrors
    }

    # Validate the complete path and size inventory before reading compressed
    # data. An unexpected archive cannot trigger decompression of extra entries.
    $inventoryErrors = New-Object System.Collections.Generic.List[string]
    foreach ($depotPath in $depotFiles.Keys) {
        if (-not $archiveFiles.ContainsKey($depotPath)) {
            $inventoryErrors.Add("missing from ZIP: '$depotPath'")
            continue
        }
        $depotRecord = $depotFiles[$depotPath]
        $archiveRecord = $archiveFiles[$depotPath]
        if (-not $archiveRecord.Path.Equals($depotRecord.Path, [System.StringComparison]::Ordinal)) {
            $inventoryErrors.Add("path casing '$($archiveRecord.Path)' != '$($depotRecord.Path)'")
        }
        if ($archiveRecord.Length -ne $depotRecord.Length) {
            $inventoryErrors.Add(
                "size '$depotPath': ZIP=$($archiveRecord.Length), depot=$($depotRecord.Length)")
        }
    }
    foreach ($archivePath in $archiveFiles.Keys) {
        if (-not $depotFiles.ContainsKey($archivePath)) {
            $inventoryErrors.Add("only in ZIP: '$archivePath'")
        }
    }
    if ($inventoryErrors.Count -gt 0) {
        Throw-ComparisonFailure -Stage 'file-list/size' -Errors $inventoryErrors
    }

    $hashErrors = New-Object System.Collections.Generic.List[string]
    $orderedPaths = [string[]]@($depotFiles.Keys)
    [System.Array]::Sort($orderedPaths, [System.StringComparer]::Ordinal)
    foreach ($relativePath in $orderedPaths) {
        $depotHash = Get-FileSha256 -Path $depotFiles[$relativePath].FullName
        $entryStream = $archiveFiles[$relativePath].Entry.Open()
        try {
            $archiveHash = Get-StreamSha256 -Stream $entryStream
        }
        finally {
            $entryStream.Dispose()
        }
        if (-not $archiveHash.Equals($depotHash, [System.StringComparison]::Ordinal)) {
            $hashErrors.Add("SHA-256 '$relativePath': ZIP=$archiveHash, depot=$depotHash")
        }
    }
    if ($hashErrors.Count -gt 0) {
        Throw-ComparisonFailure -Stage 'SHA-256' -Errors $hashErrors
    }

    $depotByteCount = [long](($depotFiles.Values | Measure-Object -Property Length -Sum).Sum)
    Write-Host "Steam ZIP matches the final staged depot."
    Write-Host "ZIP: $resolvedZipPath"
    Write-Host "Depot: $resolvedDepotRoot"
    Write-Host "Files: $($depotFiles.Count)"
    Write-Host "Uncompressed bytes: $depotByteCount"
    Write-Host "Comparison: exact path spelling, uncompressed size, SHA-256"
    Write-Warning "Integrity PASS is not release clearance. Draft/legal gates remain a NO-GO for public Steam distribution."
}
finally {
    $archive.Dispose()
}

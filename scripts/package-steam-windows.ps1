#requires -Version 5.1

[CmdletBinding(PositionalBinding = $false)]
param(
    [string]$BuildDirectory = "build-release-preflight",
    [string]$SteamOutputDirectory = "dist/steam/windows-x64",
    [ValidatePattern('^\d+\.\d+\.\d+(?:\.\d+)?(?:[-+][0-9A-Za-z.-]+)?$')]
    [string]$ExpectedVersion = "0.9.0",
    [switch]$CreateZip,
    [switch]$EngineeringCandidate
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Get-NormalizedFullPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$BasePath
    )

    $candidate = $Path
    if (-not [System.IO.Path]::IsPathRooted($candidate)) {
        $candidate = Join-Path $BasePath $candidate
    }
    return [System.IO.Path]::GetFullPath($candidate).TrimEnd('\', '/')
}

function Test-IsStrictChildPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Child,
        [Parameter(Mandatory = $true)]
        [string]$Parent
    )

    $normalizedChild = [System.IO.Path]::GetFullPath($Child).TrimEnd('\', '/')
    $normalizedParent = [System.IO.Path]::GetFullPath($Parent).TrimEnd('\', '/')
    $prefix = $normalizedParent + [System.IO.Path]::DirectorySeparatorChar
    return $normalizedChild.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)
}

function Assert-FileExists {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required file is missing: $Path"
    }
}

function Assert-DirectoryExists {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        throw "Required directory is missing: $Path"
    }
}

function Assert-ReleaseBuildConfiguration {
    param(
        [Parameter(Mandatory = $true)][string]$BuildRoot,
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$ExecutablePath
    )

    $cachePath = Join-Path $BuildRoot "CMakeCache.txt"
    Assert-FileExists $cachePath
    if (-not (Test-IsStrictChildPath -Child $ExecutablePath -Parent $BuildRoot)) {
        throw "Release executable is outside its build directory: $ExecutablePath"
    }
    $sourceLines = @(Select-String -LiteralPath $cachePath -Pattern '^CMAKE_HOME_DIRECTORY:INTERNAL=(.*)$')
    if ($sourceLines.Count -ne 1) {
        throw "Build cache must identify exactly one source checkout: $cachePath"
    }
    $cachedProjectRoot = [IO.Path]::GetFullPath($sourceLines[0].Matches[0].Groups[1].Value).TrimEnd('\', '/')
    $expectedProjectRoot = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\', '/')
    if (-not $cachedProjectRoot.Equals($expectedProjectRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Build cache belongs to a different source checkout; refusing to mix its executable with current resources: $cachedProjectRoot"
    }

    $buildTypeLines = @(Select-String -LiteralPath $cachePath -Pattern '^CMAKE_BUILD_TYPE:STRING=(.*)$')
    $multiConfigLines = @(Select-String -LiteralPath $cachePath -Pattern '^CMAKE_CONFIGURATION_TYPES:STRING=(.*)$')
    if ($multiConfigLines.Count -gt 0) {
        if ($multiConfigLines.Count -ne 1 -or
            @($multiConfigLines[0].Matches[0].Groups[1].Value -split ';') -cnotcontains "Release" -or
            (Split-Path -Leaf (Split-Path -Parent $ExecutablePath)) -cne "Release") {
            throw "A multi-configuration build must include Release and package its Release-directory executable: $ExecutablePath"
        }
    }
    elseif ($buildTypeLines.Count -ne 1 -or $buildTypeLines[0].Matches[0].Groups[1].Value -cne "Release") {
        throw "Build directory is not configured as Release: $cachePath"
    }
}

function Copy-DepotFile {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    Assert-FileExists $Source
    $destinationParent = Split-Path -Parent $Destination
    if (-not (Test-Path -LiteralPath $destinationParent -PathType Container)) {
        New-Item -ItemType Directory -Path $destinationParent -Force | Out-Null
    }
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
}

function Get-ProductionSnapshotInventory {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$VersionDirectory
    )

    $relativeDirectory = "Resources/Maps/Tilesets/Generated/FantasyNeutral/$VersionDirectory"
    $sourceRoot = [System.IO.Path]::GetFullPath((Join-Path $ProjectRoot $relativeDirectory)).TrimEnd('\', '/')
    Assert-DirectoryExists $sourceRoot
    $manifestPath = Join-Path $sourceRoot "pack_manifest.json"
    $snapshotPath = Join-Path $sourceRoot "production_style_snapshot.json"
    Assert-FileExists $manifestPath
    Assert-FileExists $snapshotPath

    $manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
    $snapshot = Get-Content -Raw -LiteralPath $snapshotPath | ConvertFrom-Json
    $assets = @($manifest.assets)
    if ($assets.Count -eq 0) {
        throw "FantasyNeutral $VersionDirectory production manifest contains no assets."
    }
    if (@($assets | Where-Object { [string]$_.status -ne "accepted" }).Count -gt 0) {
        throw "FantasyNeutral $VersionDirectory production manifest contains non-accepted assets."
    }
    if ($null -ne $manifest.PSObject.Properties["candidate_count"] -and [int]$manifest.candidate_count -ne 0) {
        throw "FantasyNeutral $VersionDirectory production manifest reports candidate assets."
    }
    if ($null -ne $manifest.PSObject.Properties["needs_repaint_count"] -and [int]$manifest.needs_repaint_count -ne 0) {
        throw "FantasyNeutral $VersionDirectory production manifest reports assets needing repaint."
    }
    if ($null -ne $manifest.PSObject.Properties["asset_count"] -and [int]$manifest.asset_count -ne $assets.Count) {
        throw "FantasyNeutral $VersionDirectory manifest asset_count does not match its asset list."
    }
    if ($null -ne $manifest.PSObject.Properties["accepted_count"] -and [int]$manifest.accepted_count -ne $assets.Count) {
        throw "FantasyNeutral $VersionDirectory manifest accepted_count does not match its asset list."
    }
    if ([string]$snapshot.state -ne "production") {
        throw "FantasyNeutral $VersionDirectory snapshot is not marked as production."
    }
    if ([string]$manifest.style_id -ne [string]$snapshot.style_id -or
        [string]$manifest.style_version -ne [string]$snapshot.style_version) {
        throw "FantasyNeutral $VersionDirectory manifest and snapshot style identities differ."
    }
    foreach ($identityField in @("atlas_version", "snapshot_id", "base_snapshot_id")) {
        $manifestIdentity = $manifest.PSObject.Properties[$identityField]
        $snapshotIdentity = $snapshot.PSObject.Properties[$identityField]
        if ($null -ne $manifestIdentity -and $null -ne $snapshotIdentity -and
            [string]$manifestIdentity.Value -ne [string]$snapshotIdentity.Value) {
            throw "FantasyNeutral $VersionDirectory manifest and snapshot $identityField values differ."
        }
    }
    if ($null -ne $snapshot.PSObject.Properties["asset_count"] -and [int]$snapshot.asset_count -ne $assets.Count) {
        throw "FantasyNeutral $VersionDirectory snapshot asset_count does not match its asset list."
    }

    $snapshotAssets = @($snapshot.assets)
    if ($snapshotAssets.Count -ne $assets.Count) {
        throw "FantasyNeutral $VersionDirectory snapshot and manifest asset counts differ."
    }
    $snapshotById = @{}
    foreach ($snapshotAsset in $snapshotAssets) {
        $snapshotId = [string]$snapshotAsset.id
        if ([string]::IsNullOrWhiteSpace($snapshotId) -or $snapshotById.ContainsKey($snapshotId)) {
            throw "FantasyNeutral $VersionDirectory snapshot contains a blank or duplicate asset ID '$snapshotId'."
        }
        $snapshotById[$snapshotId] = $snapshotAsset
    }

    $manifestIds = @{}
    $manifestFiles = @{}
    $assetRelativePaths = New-Object System.Collections.Generic.List[string]
    $expectedFiles = @{
        "pack_manifest.json" = $true
        "production_style_snapshot.json" = $true
    }
    foreach ($asset in $assets) {
        $assetId = [string]$asset.id
        if ([string]::IsNullOrWhiteSpace($assetId) -or $manifestIds.ContainsKey($assetId)) {
            throw "FantasyNeutral $VersionDirectory manifest contains a blank or duplicate asset ID '$assetId'."
        }
        $manifestIds[$assetId] = $true
        if (-not $snapshotById.ContainsKey($assetId)) {
            throw "FantasyNeutral $VersionDirectory snapshot is missing manifest asset '$assetId'."
        }

        $assetRelativePath = ([string]$asset.file).Replace('\', '/')
        if ([string]::IsNullOrWhiteSpace($assetRelativePath) -or
            [System.IO.Path]::IsPathRooted($assetRelativePath) -or
            $assetRelativePath.Split('/') -contains "..") {
            throw "FantasyNeutral $VersionDirectory manifest contains unsafe asset path '$assetRelativePath'."
        }
        $sourceAsset = [System.IO.Path]::GetFullPath((Join-Path $sourceRoot $assetRelativePath))
        if (-not (Test-IsStrictChildPath -Child $sourceAsset -Parent $sourceRoot)) {
            throw "FantasyNeutral $VersionDirectory asset path escapes its production directory: $assetRelativePath"
        }
        Assert-FileExists $sourceAsset

        $snapshotAsset = $snapshotById[$assetId]
        $snapshotRelativePath = ([string]$snapshotAsset.file).Replace('\', '/')
        if ($snapshotRelativePath -ne $assetRelativePath) {
            throw "FantasyNeutral $VersionDirectory snapshot file for '$assetId' differs from its manifest."
        }
        if ($null -ne $snapshotAsset.PSObject.Properties["status"] -and
            [string]$snapshotAsset.status -ne "accepted") {
            throw "FantasyNeutral $VersionDirectory snapshot contains non-accepted asset '$assetId'."
        }

        $assetPathKey = $assetRelativePath.ToLowerInvariant()
        if ($manifestFiles.ContainsKey($assetPathKey)) {
            throw "FantasyNeutral $VersionDirectory manifest repeats asset file '$assetRelativePath'."
        }
        $manifestFiles[$assetPathKey] = $true
        $expectedFiles[$assetPathKey] = $true
        $assetRelativePaths.Add($assetRelativePath)

        $declaredHashes = New-Object System.Collections.Generic.List[string]
        foreach ($hashOwner in @($asset, $snapshotAsset)) {
            $hashProperty = $hashOwner.PSObject.Properties["sha256"]
            if ($null -ne $hashProperty -and -not [string]::IsNullOrWhiteSpace([string]$hashProperty.Value)) {
                $declaredHashes.Add(([string]$hashProperty.Value).ToLowerInvariant())
            }
        }
        if ($declaredHashes.Count -gt 0) {
            $actualHash = (Get-FileHash -LiteralPath $sourceAsset -Algorithm SHA256).Hash.ToLowerInvariant()
            foreach ($declaredHash in $declaredHashes) {
                if ($declaredHash -notmatch '^[0-9a-f]{64}$' -or $declaredHash -ne $actualHash) {
                    throw "FantasyNeutral $VersionDirectory asset hash mismatch for '$assetId'."
                }
            }
        }
    }

    $sourceManifestHashProperty = $snapshot.PSObject.Properties["source_manifest_sha256"]
    if ($null -ne $sourceManifestHashProperty -and
        -not [string]::IsNullOrWhiteSpace([string]$sourceManifestHashProperty.Value)) {
        $declaredManifestHash = ([string]$sourceManifestHashProperty.Value).ToLowerInvariant()
        $actualManifestHash = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($declaredManifestHash -notmatch '^[0-9a-f]{64}$' -or $declaredManifestHash -ne $actualManifestHash) {
            throw "FantasyNeutral $VersionDirectory source_manifest_sha256 does not match pack_manifest.json."
        }
    }

    $manifestSelectionProperty = $manifest.PSObject.Properties["selection_overrides"]
    $snapshotSelectionProperty = $snapshot.PSObject.Properties["selection_overrides"]
    if ($null -ne $manifestSelectionProperty -or $null -ne $snapshotSelectionProperty) {
        if ($null -eq $manifestSelectionProperty -or $null -eq $snapshotSelectionProperty) {
            throw "FantasyNeutral $VersionDirectory manifest and snapshot must both declare selection_overrides."
        }
        $manifestSelectionRows = @($manifest.selection_overrides | ForEach-Object {
                $assetIds = @($_.asset_ids | ForEach-Object { [string]$_ } | Sort-Object) -join ','
                "$($_.map_level)|$($_.role)|$($_.semantic_role)|$($_.mode)|$assetIds"
            } | Sort-Object)
        $snapshotSelectionRows = @($snapshot.selection_overrides | ForEach-Object {
                $assetIds = @($_.asset_ids | ForEach-Object { [string]$_ } | Sort-Object) -join ','
                "$($_.map_level)|$($_.role)|$($_.semantic_role)|$($_.mode)|$assetIds"
            } | Sort-Object)
        if (($manifestSelectionRows -join "`n") -ne ($snapshotSelectionRows -join "`n")) {
            throw "FantasyNeutral $VersionDirectory manifest and snapshot selection policies differ."
        }
    }

    $manifestOverlayHashProperty = $manifest.PSObject.Properties["overlay_content_sha256"]
    $snapshotOverlayHashProperty = $snapshot.PSObject.Properties["overlay_content_sha256"]
    if ($null -ne $manifestOverlayHashProperty -or $null -ne $snapshotOverlayHashProperty) {
        if ($null -eq $manifestOverlayHashProperty -or $null -eq $snapshotOverlayHashProperty) {
            throw "FantasyNeutral $VersionDirectory manifest and snapshot must both declare overlay_content_sha256."
        }
        $declaredOverlayHash = ([string]$manifestOverlayHashProperty.Value).ToLowerInvariant()
        if ($declaredOverlayHash -notmatch '^[0-9a-f]{64}$' -or
            ([string]$snapshotOverlayHashProperty.Value).ToLowerInvariant() -ne $declaredOverlayHash) {
            throw "FantasyNeutral $VersionDirectory manifest and snapshot overlay_content_sha256 values differ or are invalid."
        }

        $overlayLines = New-Object System.Collections.Generic.List[string]
        $overlayLines.Add([string]$manifest.base_snapshot_id)
        @($assets | ForEach-Object {
                "$($_.id)|$(([string]$_.file).Replace('\', '/'))|$(([string]$_.sha256).ToLowerInvariant())"
            } | Sort-Object) | ForEach-Object { $overlayLines.Add($_) }
        @($manifest.selection_overrides | ForEach-Object {
                "$($_.map_level)|$($_.role)|$($_.semantic_role)|$($_.mode)"
            } | Sort-Object) | ForEach-Object { $overlayLines.Add($_) }
        $algorithm = [System.Security.Cryptography.SHA256]::Create()
        try {
            $overlayBytes = [System.Text.Encoding]::UTF8.GetBytes(($overlayLines -join "`n") + "`n")
            $actualOverlayHash = ([System.BitConverter]::ToString($algorithm.ComputeHash($overlayBytes))).Replace('-', '').ToLowerInvariant()
        }
        finally {
            $algorithm.Dispose()
        }
        if ($actualOverlayHash -ne $declaredOverlayHash) {
            throw "FantasyNeutral $VersionDirectory overlay_content_sha256 does not match its canonical manifest content."
        }
    }

    foreach ($sourceFile in @(Get-ChildItem -LiteralPath $sourceRoot -Recurse -File -Force)) {
        $relativeFile = $sourceFile.FullName.Substring($sourceRoot.Length).TrimStart('\', '/').Replace('\', '/')
        if (-not $expectedFiles.ContainsKey($relativeFile.ToLowerInvariant())) {
            throw "FantasyNeutral $VersionDirectory contains an unmanifested production file: $relativeFile"
        }
    }

    if ($expectedFiles.Count -ne @(Get-ChildItem -LiteralPath $sourceRoot -Recurse -File -Force).Count) {
        throw "FantasyNeutral $VersionDirectory production file count does not match its manifest."
    }

    $atlasVersionProperty = $snapshot.PSObject.Properties["atlas_version"]
    $baseSnapshotProperty = $snapshot.PSObject.Properties["base_snapshot_id"]
    return [pscustomobject]@{
        VersionDirectory = $VersionDirectory
        RelativeDirectory = $relativeDirectory
        SourceRoot = $sourceRoot
        ManifestPath = $manifestPath
        SnapshotPath = $snapshotPath
        AtlasVersion = if ($null -ne $atlasVersionProperty) { [string]$atlasVersionProperty.Value } else { "" }
        SnapshotId = [string]$snapshot.snapshot_id
        BaseSnapshotId = if ($null -ne $baseSnapshotProperty) { [string]$baseSnapshotProperty.Value } else { "" }
        AssetRelativePaths = $assetRelativePaths.ToArray()
    }
}

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$buildRoot = Get-NormalizedFullPath -Path $BuildDirectory -BasePath $projectRoot
$steamOutputRoot = Get-NormalizedFullPath -Path $SteamOutputDirectory -BasePath $projectRoot
$depotRoot = [System.IO.Path]::GetFullPath((Join-Path $steamOutputRoot "GMEditor-Depot")).TrimEnd('\', '/')

Assert-DirectoryExists $buildRoot
if (-not (Test-IsStrictChildPath -Child $steamOutputRoot -Parent $projectRoot)) {
    throw "Refusing a Steam output directory outside the project root: $steamOutputRoot"
}

$executableCandidates = @(@(
        (Join-Path $buildRoot "TTRPG_Editor.exe"),
        (Join-Path $buildRoot "Release/TTRPG_Editor.exe")
    ) | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })

if ($executableCandidates.Count -ne 1) {
    throw "Expected exactly one Release executable at '$buildRoot/TTRPG_Editor.exe' or '$buildRoot/Release/TTRPG_Editor.exe'; found $($executableCandidates.Count)."
}
$sourceExecutable = [System.IO.Path]::GetFullPath($executableCandidates[0])
$sourceVersionInfo = (Get-Item -LiteralPath $sourceExecutable).VersionInfo
$fileVersion = [string]$sourceVersionInfo.FileVersion
if ($fileVersion -ne $ExpectedVersion) {
    throw "Release executable version mismatch. Expected '$ExpectedVersion', got '$fileVersion': $sourceExecutable"
}
if ([string]$sourceVersionInfo.ProductName -ne "GM Editor") {
    throw "Release executable ProductName is not 'GM Editor': $sourceExecutable"
}
if ([string]$sourceVersionInfo.OriginalFilename -ne "GMEditor.exe") {
    throw "Release executable OriginalFilename is not 'GMEditor.exe': $sourceExecutable"
}
$expectedProductVersion = "$ExpectedVersion Early Access"
if ([string]$sourceVersionInfo.ProductVersion -ne $expectedProductVersion) {
    throw "Release executable ProductVersion mismatch. Expected '$expectedProductVersion': $sourceExecutable"
}
$expectedFileDescription = "GM Editor - Campaign preparation and live-session workspace"
$fileDescriptionCodePoints = @(([string]$sourceVersionInfo.FileDescription).ToCharArray() |
    ForEach-Object { [int][char]$_ })
if ([string]$sourceVersionInfo.FileDescription -ne $expectedFileDescription -or
    $fileDescriptionCodePoints -contains 0x00e2 -or
    $fileDescriptionCodePoints -contains 0x20ac -or
    $fileDescriptionCodePoints -contains 0xfffd) {
    throw "Release executable FileDescription is unexpected or contains encoding corruption: $sourceExecutable"
}
$sourceExecutableSha256 = (Get-FileHash -LiteralPath $sourceExecutable -Algorithm SHA256).Hash.ToLowerInvariant()

Assert-ReleaseBuildConfiguration -BuildRoot $buildRoot -ProjectRoot $projectRoot -ExecutablePath $sourceExecutable

# Resolve the audio package from its catalog before replacing an existing depot.
# Unknown files beside the catalog are intentionally not copied.
$audioSourceRoot = [System.IO.Path]::GetFullPath((Join-Path $projectRoot "Resources/Audio")).TrimEnd('\', '/')
$audioCatalogSource = Join-Path $audioSourceRoot "catalog.psv"
$audioAttributionSource = Join-Path $audioSourceRoot "ATTRIBUTION.md"
Assert-FileExists $audioCatalogSource
Assert-FileExists $audioAttributionSource
$audioRuntimeFiles = New-Object System.Collections.Generic.List[string]
$audioRuntimeFiles.Add("ATTRIBUTION.md")
$audioRuntimeFiles.Add("catalog.psv")
$audioSeenPaths = @{}
foreach ($line in @(Get-Content -LiteralPath $audioCatalogSource)) {
    if ([string]::IsNullOrWhiteSpace($line) -or $line.StartsWith("#")) {
        continue
    }
    $columns = @($line -split '\|')
    if ($columns.Count -ne 11) {
        throw "Audio catalog contains an invalid row: $line"
    }
    $catalogPath = ([string]$columns[4]).Replace('\', '/')
    $audioPrefix = "Resources/Audio/"
    if (-not $catalogPath.StartsWith($audioPrefix, [System.StringComparison]::Ordinal) -or
        $catalogPath.Split('/') -contains "..") {
        throw "Audio catalog references a file outside Resources/Audio: $catalogPath"
    }
    $audioRelativePath = $catalogPath.Substring($audioPrefix.Length)
    $audioKey = $audioRelativePath.ToLowerInvariant()
    if ($audioSeenPaths.ContainsKey($audioKey)) {
        throw "Audio catalog contains duplicate runtime path: $catalogPath"
    }
    $audioSeenPaths[$audioKey] = $true
    $audioSource = [System.IO.Path]::GetFullPath((Join-Path $audioSourceRoot $audioRelativePath))
    if (-not (Test-IsStrictChildPath -Child $audioSource -Parent $audioSourceRoot)) {
        throw "Audio catalog path escapes its runtime root: $catalogPath"
    }
    Assert-FileExists $audioSource
    $audioRuntimeFiles.Add($audioRelativePath)
}

$sampleNoticeSource = Join-Path $projectRoot "Resources/Samples/ONBOARDING_SAMPLE_LICENSE.md"
$styleManifestSource = Join-Path $projectRoot "Resources/Maps/Tilesets/Styles/fantasy_neutral.json"
Assert-FileExists $sampleNoticeSource
Assert-FileExists $styleManifestSource

# Inter 4.1 is deliberately packaged from an exact, reviewed static-font set.
# Refuse locally replaced or re-encoded upstream files before touching a depot.
$interFontSourceRoot = Join-Path $projectRoot "Resources/Fonts/Inter"
$interFontExpectedHashes = [ordered]@{
    "Inter-Regular.ttf"  = "40d692fce188e4471e2b3cba937be967878f631ad3ebbbdcd587687c7ebe0c82"
    "Inter-SemiBold.ttf" = "78a843fade9d4612a5567302fb595b56976eb5fcebf4fea5a5912d638bafcde3"
    "LICENSE.txt"        = "262481e844521b326f5ecd053e59b98c8b2da78c8ee1bdbb6e8174305e54935a"
}
foreach ($fontEntry in $interFontExpectedHashes.GetEnumerator()) {
    $fontPath = Join-Path $interFontSourceRoot $fontEntry.Key
    Assert-FileExists $fontPath
    $fontHash = (Get-FileHash -LiteralPath $fontPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($fontHash -ne $fontEntry.Value) {
        throw "Inter 4.1 runtime file hash mismatch: $fontPath"
    }
}
$interFontProvenanceSource = Join-Path $interFontSourceRoot "SOURCE.md"
Assert-FileExists $interFontProvenanceSource

$atlasFontSourceRoot = Join-Path $projectRoot "Resources/Fonts/SourceSerif4"
$atlasFontExpectedHashes = [ordered]@{
    "SourceSerif4-Regular.ttf" = "e5a4ee6a3d87bb9024796be390c6771e2a0eb1883dae25effaf57ca01668e24b"
    "LICENSE.md" = "75784a295293a8992f5a8d99210566e0064a012e6dab6731305e3787f15896c7"
}
foreach ($fontEntry in $atlasFontExpectedHashes.GetEnumerator()) {
    $fontPath = Join-Path $atlasFontSourceRoot $fontEntry.Key
    Assert-FileExists $fontPath
    if ((Get-FileHash -LiteralPath $fontPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $fontEntry.Value) {
        throw "Source Serif 4.005 runtime file hash mismatch: $fontPath"
    }
}
Assert-FileExists (Join-Path $atlasFontSourceRoot "SOURCE.md")

. (Join-Path $PSScriptRoot 'portrait-runtime-inventory.ps1')
$portraitSourceRoot = Join-Path $projectRoot 'Resources/Portraits'
$portraitRuntimeFiles = @(Get-PortraitRuntimeInventory -Root $portraitSourceRoot)
foreach ($portraitFile in $portraitRuntimeFiles) {Assert-FileExists (Join-Path $portraitSourceRoot $portraitFile)}
$nativePortraitHeader = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'Models/Character/PortraitArtwork.hpp')
$nativePortraitNames = @([regex]::Matches($nativePortraitHeader,'"([a-z_]+\.png)"') | ForEach-Object {$_.Groups[1].Value} | Sort-Object)
$packedPortraitNames = @($portraitRuntimeFiles | Where-Object {$_ -like 'Company/*.png'} | ForEach-Object {$_.Substring(8)} | Sort-Object)
if ($nativePortraitNames.Count -ne 47 -or (Compare-Object $nativePortraitNames $packedPortraitNames)) {throw 'Portrait package inventory does not match the native species table.'}

# Resolve and validate every production snapshot before replacing an existing
# depot. Only manifest-listed assets are copied below.
$productionSnapshotInventories = @(
    foreach ($versionDirectory in @("v3", "v3_1", "v3_2", "v3_3")) {
        Get-ProductionSnapshotInventory -ProjectRoot $projectRoot -VersionDirectory $versionDirectory
    }
)
$v3Inventory = @($productionSnapshotInventories | Where-Object VersionDirectory -eq "v3")
$v31Inventory = @($productionSnapshotInventories | Where-Object VersionDirectory -eq "v3_1")
$v32Inventory = @($productionSnapshotInventories | Where-Object VersionDirectory -eq "v3_2")
$v33Inventory = @($productionSnapshotInventories | Where-Object VersionDirectory -eq "v3_3")
if ($v3Inventory.Count -ne 1 -or $v31Inventory.Count -ne 1 -or $v32Inventory.Count -ne 1 -or
    $v33Inventory.Count -ne 1 -or
    $v31Inventory[0].AtlasVersion -ne "3.1" -or
    $v31Inventory[0].BaseSnapshotId -ne $v3Inventory[0].SnapshotId -or
    $v32Inventory[0].AtlasVersion -ne "3.2" -or
    $v32Inventory[0].BaseSnapshotId -ne $v31Inventory[0].SnapshotId -or
    $v33Inventory[0].AtlasVersion -ne "3.3" -or
    $v33Inventory[0].BaseSnapshotId -ne $v32Inventory[0].SnapshotId) {
    throw "FantasyNeutral V3/V3.1/V3.2/V3.3 production snapshot lineage is inconsistent."
}

if (-not (Test-IsStrictChildPath -Child $depotRoot -Parent $steamOutputRoot)) {
    throw "Refusing unsafe depot target outside the fixed Steam output root: $depotRoot"
}
if ((Split-Path -Leaf $depotRoot) -ne "GMEditor-Depot") {
    throw "Refusing to replace an unexpected depot directory: $depotRoot"
}

if (Test-Path -LiteralPath $depotRoot) {
    $resolvedExistingDepot = (Resolve-Path -LiteralPath $depotRoot).Path.TrimEnd('\', '/')
    if (-not $resolvedExistingDepot.Equals($depotRoot, [System.StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-IsStrictChildPath -Child $resolvedExistingDepot -Parent $steamOutputRoot)) {
        throw "Refusing to remove an unresolved or redirected depot path: $resolvedExistingDepot"
    }
    Remove-Item -LiteralPath $resolvedExistingDepot -Recurse -Force
}
New-Item -ItemType Directory -Path $depotRoot -Force | Out-Null

# Customer-facing executable name. The internal build target remains unchanged.
Copy-DepotFile -Source $sourceExecutable -Destination (Join-Path $depotRoot "GMEditor.exe")

# A packaged build must carry this marker so ApplicationPaths does not mistake a
# depot staged inside a source checkout for a developer/portable installation.
Copy-DepotFile `
    -Source (Join-Path $projectRoot "distribution/gm_editor.install.json") `
    -Destination (Join-Path $depotRoot "gm_editor.install.json")

# Copy only explicitly cataloged runtime files. Campaigns, backups, recovery
# snapshots, source art, QA reports and prototype packs are never traversed.
foreach ($audioRelativePath in $audioRuntimeFiles) {
    Copy-DepotFile `
        -Source (Join-Path $audioSourceRoot $audioRelativePath) `
        -Destination (Join-Path $depotRoot "Resources/Audio/$audioRelativePath")
}
Copy-DepotFile `
    -Source $sampleNoticeSource `
    -Destination (Join-Path $depotRoot "Resources/Samples/ONBOARDING_SAMPLE_LICENSE.md")
foreach ($fontName in @($interFontExpectedHashes.Keys) + @("SOURCE.md")) {
    Copy-DepotFile `
        -Source (Join-Path $interFontSourceRoot $fontName) `
        -Destination (Join-Path $depotRoot "Resources/Fonts/Inter/$fontName")
}
foreach ($fontName in @($atlasFontExpectedHashes.Keys) + @("SOURCE.md")) {
    Copy-DepotFile `
        -Source (Join-Path $atlasFontSourceRoot $fontName) `
        -Destination (Join-Path $depotRoot "Resources/Fonts/SourceSerif4/$fontName")
}
foreach ($portraitFile in $portraitRuntimeFiles) {
    Copy-DepotFile -Source (Join-Path $portraitSourceRoot $portraitFile) -Destination (Join-Path $depotRoot "Resources/Portraits/$portraitFile")
}
Copy-DepotFile `
    -Source $styleManifestSource `
    -Destination (Join-Path $depotRoot "Resources/Maps/Tilesets/Styles/fantasy_neutral.json")

$v2SourceRoot = Join-Path $projectRoot "Resources/Maps/Tilesets/Generated/FantasyNeutral/v2"
$v2DestinationRoot = Join-Path $depotRoot "Resources/Maps/Tilesets/Generated/FantasyNeutral/v2"
$v2RuntimeManifestPath = Join-Path $v2SourceRoot "packed_runtime_manifest.json"
Assert-FileExists $v2RuntimeManifestPath
$v2RuntimeManifest = Get-Content -Raw -LiteralPath $v2RuntimeManifestPath | ConvertFrom-Json
$v2RuntimeAssets = @($v2RuntimeManifest.assets)
if ($v2RuntimeAssets.Count -eq 0) {
    throw "The FantasyNeutral V2 runtime manifest contains no assets."
}
$nonAcceptedV2Assets = @($v2RuntimeAssets | Where-Object { $_.status -ne "accepted" })
if ($nonAcceptedV2Assets.Count -gt 0) {
    throw "The FantasyNeutral V2 runtime manifest contains $($nonAcceptedV2Assets.Count) non-accepted assets."
}

Copy-DepotFile `
    -Source $v2RuntimeManifestPath `
    -Destination (Join-Path $v2DestinationRoot "packed_runtime_manifest.json")

$v2AtlasPrefix = "Resources/Maps/Tilesets/Generated/FantasyNeutral/v2/atlases/"
$v2AtlasPaths = @($v2RuntimeAssets | ForEach-Object { [string]$_.atlas_path } | Sort-Object -Unique)
foreach ($atlasRelativePath in $v2AtlasPaths) {
    $normalizedAtlasRelativePath = $atlasRelativePath.Replace('\', '/')
    if (-not $normalizedAtlasRelativePath.StartsWith($v2AtlasPrefix, [System.StringComparison]::Ordinal) -or
        -not $normalizedAtlasRelativePath.EndsWith(".png", [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "V2 runtime manifest references a non-runtime atlas path: $atlasRelativePath"
    }

    $sourceAtlas = [System.IO.Path]::GetFullPath((Join-Path $projectRoot $normalizedAtlasRelativePath))
    if (-not (Test-IsStrictChildPath -Child $sourceAtlas -Parent $v2SourceRoot)) {
        throw "V2 atlas reference escapes its generated runtime directory: $atlasRelativePath"
    }
    Copy-DepotFile `
        -Source $sourceAtlas `
        -Destination (Join-Path $depotRoot $normalizedAtlasRelativePath)
}

foreach ($inventory in $productionSnapshotInventories) {
    $snapshotDestinationRoot = Join-Path $depotRoot $inventory.RelativeDirectory
    Copy-DepotFile `
        -Source $inventory.ManifestPath `
        -Destination (Join-Path $snapshotDestinationRoot "pack_manifest.json")
    Copy-DepotFile `
        -Source $inventory.SnapshotPath `
        -Destination (Join-Path $snapshotDestinationRoot "production_style_snapshot.json")
    foreach ($assetRelativePath in @($inventory.AssetRelativePaths)) {
        Copy-DepotFile `
            -Source (Join-Path $inventory.SourceRoot $assetRelativePath) `
            -Destination (Join-Path $snapshotDestinationRoot $assetRelativePath)
    }
}

$documentationRoot = Join-Path $depotRoot "Documentation"
$releaseDocuments = @(
    "PRIVACY_DE_EN.md",
    "KNOWN_ISSUES_0_9_DE_EN.md",
    "THIRD_PARTY_NOTICES_DRAFT.md"
)
foreach ($documentName in $releaseDocuments) {
    Copy-DepotFile `
        -Source (Join-Path $projectRoot "docs/release/$documentName") `
        -Destination (Join-Path $documentationRoot $documentName)
}

# Preserve the upstream license texts that are available in the exact build tree.
Copy-DepotFile `
    -Source (Join-Path $projectRoot "extern/imgui/LICENSE.txt") `
    -Destination (Join-Path $documentationRoot "Licenses/DearImGui-MIT.txt")
Copy-DepotFile `
    -Source (Join-Path $projectRoot "extern/glfw/LICENSE.md") `
    -Destination (Join-Path $documentationRoot "Licenses/GLFW-zlib.txt")
$sqliteCppLicense = Join-Path $buildRoot "_deps/sqlitecpp-src/LICENSE.txt"
Copy-DepotFile `
    -Source $sqliteCppLicense `
    -Destination (Join-Path $documentationRoot "Licenses/SQLiteCpp-MIT.txt")

$readmeLines = @(
    "GM Editor $fileVersion Early Access - Windows x64",
    "",
    "Start: GMEditor.exe",
    "User data: %LOCALAPPDATA%\GM Editor (unless a documented development override is set).",
    "",
    "Deutsch:",
    "Dieses Verzeichnis ist ein technisch geprueftes Depot-Staging. Vor einer",
    "oeffentlichen Veroeffentlichung muessen die offenen Rechte- und Release-Gates",
    "in Documentation\THIRD_PARTY_NOTICES_DRAFT.md und",
    "Documentation\KNOWN_ISSUES_0_9_DE_EN.md abgeschlossen werden.",
    "",
    "English:",
    "This directory is a technically validated depot staging build. Before public",
    "distribution, close the outstanding rights and release gates documented in",
    "Documentation\THIRD_PARTY_NOTICES_DRAFT.md and",
    "Documentation\KNOWN_ISSUES_0_9_DE_EN.md."
)
[System.IO.File]::WriteAllLines(
    (Join-Path $depotRoot "README_DE_EN.txt"),
    $readmeLines,
    (New-Object System.Text.UTF8Encoding($false)))

$packageKind = if ($EngineeringCandidate) { 'engineering_candidate' } else { 'release_candidate' }
$versionLines = @(
    "manifest_schema=1",
    "product=GM Editor",
    "version=$fileVersion",
    "channel=Early Access",
    "platform=windows-x64",
    "executable=GMEditor.exe",
    "binary_sha256=$sourceExecutableSha256",
    "package_kind=$packageKind"
)
[System.IO.File]::WriteAllLines(
    (Join-Path $depotRoot "VERSION.txt"),
    $versionLines,
    (New-Object System.Text.UTF8Encoding($false)))

# The checksum list intentionally excludes itself. Paths are relative to the
# depot root and use forward slashes for deterministic comparison.
$checksumPath = Join-Path $depotRoot "SHA256SUMS.txt"
$checksumLines = Get-ChildItem -LiteralPath $depotRoot -Recurse -File |
    Where-Object { $_.FullName -ne $checksumPath } |
    ForEach-Object {
        $relativePath = $_.FullName.Substring($depotRoot.Length).TrimStart('\', '/').Replace('\', '/')
        $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        [PSCustomObject]@{ RelativePath = $relativePath; Line = "$hash  $relativePath" }
    } |
    Sort-Object RelativePath |
    ForEach-Object { $_.Line }
[System.IO.File]::WriteAllLines(
    $checksumPath,
    @($checksumLines),
    (New-Object System.Text.UTF8Encoding($false)))

$assertScript = Join-Path $PSScriptRoot "assert-steam-depot.ps1"
Assert-FileExists $assertScript
& $assertScript -DepotRoot $depotRoot -ExpectedPackageKind $packageKind

$zipPath = $null
$zipChecksumPath = $null
$zipSha256 = $null
if ($CreateZip) {
    $safeVersion = ($fileVersion -replace '[^0-9A-Za-z._-]', '-')
    $zipPath = Join-Path $steamOutputRoot "GMEditor-$safeVersion-windows-x64.zip"
    $normalizedZipPath = [System.IO.Path]::GetFullPath($zipPath)
    if (-not (Test-IsStrictChildPath -Child $normalizedZipPath -Parent $steamOutputRoot) -or
        [System.IO.Path]::GetExtension($normalizedZipPath) -ne ".zip") {
        throw "Refusing unsafe ZIP target: $normalizedZipPath"
    }
    if (Test-Path -LiteralPath $normalizedZipPath) {
        Remove-Item -LiteralPath $normalizedZipPath -Force
    }
    Compress-Archive -Path (Join-Path $depotRoot "*") -DestinationPath $normalizedZipPath -CompressionLevel Optimal
    $zipIntegrityScript = Join-Path $PSScriptRoot "assert-steam-zip-matches-depot.ps1"
    Assert-FileExists $zipIntegrityScript
    & $zipIntegrityScript -ZipPath $normalizedZipPath -DepotRoot $depotRoot -ExpectedPackageKind $packageKind
    $zipPath = $normalizedZipPath
    $zipSha256 = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $zipChecksumPath = $zipPath + ".sha256"
    [System.IO.File]::WriteAllText(
        $zipChecksumPath,
        "$zipSha256  $(Split-Path -Leaf $zipPath)`n",
        (New-Object System.Text.UTF8Encoding($false)))
}

$depotFiles = @(Get-ChildItem -LiteralPath $depotRoot -Recurse -File)
$depotBytes = ($depotFiles | Measure-Object -Property Length -Sum).Sum
Write-Host "Steam depot staging created successfully."
Write-Host "Depot: $depotRoot"
Write-Host "Files: $($depotFiles.Count)"
Write-Host "Bytes: $depotBytes"
Write-Warning "This package is marked $packageKind and includes draft legal notices. It is not cleared for public distribution."
if ($null -ne $zipPath) {
    Write-Host "ZIP: $zipPath"
    Write-Host "ZIP SHA-256: $zipSha256"
    Write-Host "ZIP checksum file: $zipChecksumPath"
}

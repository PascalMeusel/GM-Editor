#requires -Version 5.1

[CmdletBinding(PositionalBinding = $false)]
param(
    [Parameter(Mandatory = $true)]
    [string]$DepotRoot,
    [switch]$AllowAppLocalRuntime,
    [ValidateSet('release_candidate','engineering_candidate')][string]$ExpectedPackageKind = 'release_candidate'
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Assert-DepotFile {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$RelativePath
    )
    $path = Join-Path $Root $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Steam depot is missing required file: $RelativePath"
    }
}

function Get-TextSha256 {
    param([Parameter(Mandatory = $true)][string]$Value)

    $algorithm = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($Value)
        return ([System.BitConverter]::ToString($algorithm.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $algorithm.Dispose()
    }
}

function Assert-OnlyChildDirectories {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string[]]$AllowedNames
    )

    if (-not (Test-Path -LiteralPath $Root -PathType Container)) {
        throw "Steam depot is missing required directory: $Root"
    }
    $unexpected = @(Get-ChildItem -LiteralPath $Root -Directory -Force |
        Where-Object { $AllowedNames -notcontains $_.Name })
    if ($unexpected.Count -gt 0) {
        throw "Steam depot contains unexpected resource directory: $($unexpected[0].FullName)"
    }
}

function Assert-ExactFileSet {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string[]]$ExpectedRelativePaths
    )

    if (-not (Test-Path -LiteralPath $Root -PathType Container)) {
        throw "Steam depot is missing required directory: $Root"
    }
    $normalizedRoot = [System.IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    $expected = @{}
    foreach ($relativePathValue in $ExpectedRelativePaths) {
        $relativePath = ([string]$relativePathValue).Replace('\', '/')
        if ([string]::IsNullOrWhiteSpace($relativePath) -or
            [System.IO.Path]::IsPathRooted($relativePath) -or
            $relativePath.Split('/') -contains "..") {
            throw "Exact depot file allowlist contains unsafe path: $relativePathValue"
        }
        $key = $relativePath.ToLowerInvariant()
        if ($expected.ContainsKey($key)) {
            throw "Exact depot file allowlist contains duplicate path: $relativePath"
        }
        $expected[$key] = $relativePath
    }

    $actual = @{}
    foreach ($file in @(Get-ChildItem -LiteralPath $normalizedRoot -Recurse -File -Force)) {
        $relativePath = $file.FullName.Substring($normalizedRoot.Length).TrimStart('\', '/').Replace('\', '/')
        $key = $relativePath.ToLowerInvariant()
        if (-not $expected.ContainsKey($key)) {
            throw "Steam depot contains a file outside the exact runtime allowlist: $($file.FullName)"
        }
        $actual[$key] = $true
    }
    foreach ($entry in $expected.GetEnumerator()) {
        if (-not $actual.ContainsKey($entry.Key)) {
            throw "Steam depot is missing allowlisted runtime file: $(Join-Path $normalizedRoot $entry.Value)"
        }
    }
}

function Assert-ProductionManifest {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$RelativeDirectory
    )

    $manifestPath = Join-Path $Root "$RelativeDirectory/pack_manifest.json"
    $snapshotPath = Join-Path $Root "$RelativeDirectory/production_style_snapshot.json"
    Assert-DepotFile -Root $Root -RelativePath "$RelativeDirectory/pack_manifest.json"
    Assert-DepotFile -Root $Root -RelativePath "$RelativeDirectory/production_style_snapshot.json"

    $manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
    $snapshot = Get-Content -Raw -LiteralPath $snapshotPath | ConvertFrom-Json
    $assets = @($manifest.assets)
    if ($assets.Count -eq 0) {
        throw "Production manifest contains no assets: $manifestPath"
    }
    if (@($assets | Where-Object { $_.status -ne "accepted" }).Count -gt 0) {
        throw "Production manifest contains non-accepted assets: $manifestPath"
    }
    if ($null -ne $manifest.PSObject.Properties["candidate_count"] -and [int]$manifest.candidate_count -ne 0) {
        throw "Production manifest reports candidate assets: $manifestPath"
    }
    if ($null -ne $manifest.PSObject.Properties["needs_repaint_count"] -and [int]$manifest.needs_repaint_count -ne 0) {
        throw "Production manifest reports assets needing repaint: $manifestPath"
    }
    if ($null -ne $manifest.PSObject.Properties["asset_count"] -and [int]$manifest.asset_count -ne $assets.Count) {
        throw "Production manifest asset_count does not match its asset list: $manifestPath"
    }
    if ($null -ne $manifest.PSObject.Properties["accepted_count"] -and [int]$manifest.accepted_count -ne $assets.Count) {
        throw "Production manifest accepted_count does not match its asset list: $manifestPath"
    }
    if ([string]$snapshot.state -ne "production") {
        throw "Style snapshot is not marked as production: $snapshotPath"
    }
    if ([string]$manifest.style_id -ne [string]$snapshot.style_id -or
        [string]$manifest.style_version -ne [string]$snapshot.style_version) {
        throw "Style snapshot identity does not match its manifest: $RelativeDirectory"
    }
    foreach ($identityField in @("atlas_version", "snapshot_id", "base_snapshot_id")) {
        $manifestIdentity = $manifest.PSObject.Properties[$identityField]
        $snapshotIdentity = $snapshot.PSObject.Properties[$identityField]
        if ($null -ne $manifestIdentity -and $null -ne $snapshotIdentity -and
            [string]$manifestIdentity.Value -ne [string]$snapshotIdentity.Value) {
            throw "Style snapshot $identityField does not match its manifest: $RelativeDirectory"
        }
    }
    if ($null -ne $snapshot.PSObject.Properties["asset_count"] -and
        [int]$snapshot.asset_count -ne $assets.Count) {
        throw "Style snapshot asset_count does not match its asset list: $snapshotPath"
    }

    $snapshotAssets = @($snapshot.assets)
    if ($snapshotAssets.Count -ne $assets.Count) {
        throw "Style snapshot and manifest asset counts differ: $RelativeDirectory"
    }
    $snapshotIds = @{}
    foreach ($asset in $snapshotAssets) {
        $snapshotId = [string]$asset.id
        if ([string]::IsNullOrWhiteSpace($snapshotId) -or $snapshotIds.ContainsKey($snapshotId)) {
            throw "Style snapshot contains a blank or duplicate asset ID '$snapshotId': $snapshotPath"
        }
        $snapshotIds[$snapshotId] = $asset
    }
    $manifestIds = @{}
    $manifestFiles = @{}
    $expectedFiles = @{
        "pack_manifest.json" = $true
        "production_style_snapshot.json" = $true
    }
    foreach ($asset in $assets) {
        $assetId = [string]$asset.id
        if ([string]::IsNullOrWhiteSpace($assetId) -or $manifestIds.ContainsKey($assetId)) {
            throw "Production manifest contains a blank or duplicate asset ID '$assetId': $manifestPath"
        }
        $manifestIds[$assetId] = $true
        if (-not $snapshotIds.ContainsKey($assetId)) {
            throw "Production snapshot does not include manifest asset '$assetId': $RelativeDirectory"
        }
        $assetRelativePath = ([string]$asset.file).Replace('\', '/')
        if ([string]::IsNullOrWhiteSpace($assetRelativePath) -or
            [System.IO.Path]::IsPathRooted($assetRelativePath) -or
            $assetRelativePath.Split('/') -contains "..") {
            throw "Production manifest contains unsafe asset path '$assetRelativePath': $manifestPath"
        }
        $snapshotAsset = $snapshotIds[$assetId]
        $snapshotRelativePath = ([string]$snapshotAsset.file).Replace('\', '/')
        if ($snapshotRelativePath -ne $assetRelativePath) {
            throw "Production snapshot file for '$assetId' does not match its manifest: $RelativeDirectory"
        }
        if ($null -ne $snapshotAsset.PSObject.Properties["status"] -and
            [string]$snapshotAsset.status -ne "accepted") {
            throw "Production snapshot contains non-accepted asset '$assetId': $snapshotPath"
        }
        $assetPathKey = $assetRelativePath.ToLowerInvariant()
        if ($manifestFiles.ContainsKey($assetPathKey)) {
            throw "Production manifest references the same asset file more than once: $assetRelativePath"
        }
        $manifestFiles[$assetPathKey] = $true
        $expectedFiles[$assetPathKey] = $true
        Assert-DepotFile -Root $Root -RelativePath "$RelativeDirectory/$assetRelativePath"

        $assetPath = Join-Path (Join-Path $Root $RelativeDirectory) $assetRelativePath
        $declaredHashes = New-Object System.Collections.Generic.List[string]
        foreach ($hashOwner in @($asset, $snapshotAsset)) {
            $hashProperty = $hashOwner.PSObject.Properties["sha256"]
            if ($null -ne $hashProperty -and -not [string]::IsNullOrWhiteSpace([string]$hashProperty.Value)) {
                $declaredHashes.Add(([string]$hashProperty.Value).ToLowerInvariant())
            }
        }
        if ($declaredHashes.Count -gt 0) {
            $actualHash = (Get-FileHash -LiteralPath $assetPath -Algorithm SHA256).Hash.ToLowerInvariant()
            foreach ($declaredHash in $declaredHashes) {
                if ($declaredHash -notmatch '^[0-9a-f]{64}$' -or $declaredHash -ne $actualHash) {
                    throw "Production asset hash mismatch for '$assetId': $RelativeDirectory"
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
            throw "Production snapshot source_manifest_sha256 does not match its manifest: $RelativeDirectory"
        }
    }

    $manifestSelectionProperty = $manifest.PSObject.Properties["selection_overrides"]
    $snapshotSelectionProperty = $snapshot.PSObject.Properties["selection_overrides"]
    if ($null -ne $manifestSelectionProperty -or $null -ne $snapshotSelectionProperty) {
        if ($null -eq $manifestSelectionProperty -or $null -eq $snapshotSelectionProperty) {
            throw "Production manifest and snapshot must both declare selection_overrides: $RelativeDirectory"
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
            throw "Production manifest and snapshot selection policies differ: $RelativeDirectory"
        }
    }

    $manifestOverlayHashProperty = $manifest.PSObject.Properties["overlay_content_sha256"]
    $snapshotOverlayHashProperty = $snapshot.PSObject.Properties["overlay_content_sha256"]
    if ($null -ne $manifestOverlayHashProperty -or $null -ne $snapshotOverlayHashProperty) {
        if ($null -eq $manifestOverlayHashProperty -or $null -eq $snapshotOverlayHashProperty) {
            throw "Production manifest and snapshot must both declare overlay_content_sha256: $RelativeDirectory"
        }
        $declaredOverlayHash = ([string]$manifestOverlayHashProperty.Value).ToLowerInvariant()
        if ($declaredOverlayHash -notmatch '^[0-9a-f]{64}$' -or
            ([string]$snapshotOverlayHashProperty.Value).ToLowerInvariant() -ne $declaredOverlayHash) {
            throw "Production manifest and snapshot overlay_content_sha256 values differ or are invalid: $RelativeDirectory"
        }

        $overlayLines = New-Object System.Collections.Generic.List[string]
        $overlayLines.Add([string]$manifest.base_snapshot_id)
        @($assets | ForEach-Object {
                "$($_.id)|$(([string]$_.file).Replace('\', '/'))|$(([string]$_.sha256).ToLowerInvariant())"
            } | Sort-Object) | ForEach-Object { $overlayLines.Add($_) }
        @($manifest.selection_overrides | ForEach-Object {
                "$($_.map_level)|$($_.role)|$($_.semantic_role)|$($_.mode)"
            } | Sort-Object) | ForEach-Object { $overlayLines.Add($_) }
        $actualOverlayHash = Get-TextSha256 (($overlayLines -join "`n") + "`n")
        if ($actualOverlayHash -ne $declaredOverlayHash) {
            throw "Production overlay_content_sha256 does not match canonical manifest content: $RelativeDirectory"
        }
    }

    $manifestDirectory = [System.IO.Path]::GetFullPath((Join-Path $Root $RelativeDirectory)).TrimEnd('\', '/')
    foreach ($file in @(Get-ChildItem -LiteralPath $manifestDirectory -Recurse -File -Force)) {
        $relativeFile = $file.FullName.Substring($manifestDirectory.Length).TrimStart('\', '/').Replace('\', '/')
        if (-not $expectedFiles.ContainsKey($relativeFile.ToLowerInvariant())) {
            throw "Production snapshot directory contains an unmanifested file: $RelativeDirectory/$relativeFile"
        }
    }
}

if (-not (Test-Path -LiteralPath $DepotRoot -PathType Container)) {
    throw "Steam depot root does not exist: $DepotRoot"
}
$root = (Resolve-Path -LiteralPath $DepotRoot).Path.TrimEnd('\', '/')
$reparsePoints = @(Get-ChildItem -LiteralPath $root -Recurse -Force |
    Where-Object { ($_.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0 })
if ($reparsePoints.Count -gt 0) {
    throw "Steam depot contains a symbolic link, junction, or other reparse point: $($reparsePoints[0].FullName)"
}

$requiredFiles = @(
    "GMEditor.exe",
    "gm_editor.install.json",
    "README_DE_EN.txt",
    "VERSION.txt",
    "SHA256SUMS.txt",
    "Documentation/PRIVACY_DE_EN.md",
    "Documentation/KNOWN_ISSUES_0_9_DE_EN.md",
    "Documentation/THIRD_PARTY_NOTICES_DRAFT.md",
    "Documentation/Licenses/DearImGui-MIT.txt",
    "Documentation/Licenses/GLFW-zlib.txt",
    "Documentation/Licenses/SQLiteCpp-MIT.txt",
    "Resources/Audio/ATTRIBUTION.md",
    "Resources/Audio/catalog.psv",
    "Resources/Samples/ONBOARDING_SAMPLE_LICENSE.md",
    "Resources/Fonts/Inter/Inter-Regular.ttf",
    "Resources/Fonts/Inter/Inter-SemiBold.ttf",
    "Resources/Fonts/Inter/LICENSE.txt",
    "Resources/Fonts/Inter/SOURCE.md",
    "Resources/Fonts/SourceSerif4/SourceSerif4-Regular.ttf",
    "Resources/Fonts/SourceSerif4/LICENSE.md",
    "Resources/Fonts/SourceSerif4/SOURCE.md",
    "Resources/Portraits/runtime_manifest.json",
    "Resources/Maps/Tilesets/Styles/fantasy_neutral.json",
    "Resources/Maps/Tilesets/Generated/FantasyNeutral/v2/packed_runtime_manifest.json",
    "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3/pack_manifest.json",
    "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3/production_style_snapshot.json",
    "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3_1/pack_manifest.json",
    "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3_1/production_style_snapshot.json",
    "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3_2/pack_manifest.json",
    "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3_2/production_style_snapshot.json",
    "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3_3/pack_manifest.json",
    "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3_3/production_style_snapshot.json"
)
foreach ($relativePath in $requiredFiles) {
    Assert-DepotFile -Root $root -RelativePath $relativePath
}

$installMarker = Get-Content -Raw -LiteralPath (Join-Path $root "gm_editor.install.json") | ConvertFrom-Json
if ([string]$installMarker.product -ne "GM Editor" -or [string]$installMarker.data_policy -ne "per-user") {
    throw "gm_editor.install.json does not declare the expected GM Editor per-user data policy."
}

$versionMetadata = @{}
foreach ($line in @(Get-Content -LiteralPath (Join-Path $root "VERSION.txt"))) {
    if ($line -notmatch '^([^=]+)=(.*)$') {
        throw "VERSION.txt contains an invalid line: $line"
    }
    $key = $Matches[1]
    if ($versionMetadata.ContainsKey($key)) {
        throw "VERSION.txt contains duplicate key: $key"
    }
    $versionMetadata[$key] = $Matches[2]
}
foreach ($requiredKey in @("manifest_schema", "product", "version", "channel", "platform", "executable", "binary_sha256", "package_kind")) {
    if (-not $versionMetadata.ContainsKey($requiredKey) -or [string]::IsNullOrWhiteSpace([string]$versionMetadata[$requiredKey])) {
        throw "VERSION.txt is missing required key: $requiredKey"
    }
}
if ([string]$versionMetadata["manifest_schema"] -ne "1" -or
    [string]$versionMetadata["product"] -ne "GM Editor" -or
    [string]$versionMetadata["channel"] -ne "Early Access" -or
    [string]$versionMetadata["platform"] -ne "windows-x64" -or
    [string]$versionMetadata["executable"] -ne "GMEditor.exe" -or
    [string]$versionMetadata["package_kind"] -ne $ExpectedPackageKind) {
    throw "VERSION.txt does not declare the expected GM Editor Windows Early Access release-candidate package."
}
if ([string]$versionMetadata["version"] -notmatch '^\d+\.\d+\.\d+(?:\.\d+)?(?:[-+][0-9A-Za-z.-]+)?$') {
    throw "VERSION.txt contains an invalid semantic version: $($versionMetadata['version'])"
}
if ([string]$versionMetadata["binary_sha256"] -notmatch '^[0-9a-fA-F]{64}$') {
    throw "VERSION.txt contains an invalid binary_sha256."
}
$depotExecutable = Join-Path $root "GMEditor.exe"
$actualBinarySha256 = (Get-FileHash -LiteralPath $depotExecutable -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualBinarySha256 -ne ([string]$versionMetadata["binary_sha256"]).ToLowerInvariant()) {
    throw "VERSION.txt binary_sha256 does not match GMEditor.exe."
}
$depotVersionInfo = (Get-Item -LiteralPath $depotExecutable).VersionInfo
if ([string]$depotVersionInfo.ProductName -ne "GM Editor" -or
    [string]$depotVersionInfo.OriginalFilename -ne "GMEditor.exe" -or
    [string]$depotVersionInfo.FileVersion -ne [string]$versionMetadata["version"] -or
    [string]$depotVersionInfo.ProductVersion -ne "$($versionMetadata['version']) Early Access" -or
    [string]$depotVersionInfo.FileDescription -ne "GM Editor - Campaign preparation and live-session workspace") {
    throw "GMEditor.exe version-resource metadata does not match VERSION.txt."
}

$audioExpectedFiles = New-Object System.Collections.Generic.List[string]
$audioExpectedFiles.Add("ATTRIBUTION.md")
$audioExpectedFiles.Add("catalog.psv")
foreach ($line in @(Get-Content -LiteralPath (Join-Path $root "Resources/Audio/catalog.psv"))) {
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
    $audioExpectedFiles.Add($catalogPath.Substring($audioPrefix.Length))
}
Assert-ExactFileSet -Root (Join-Path $root "Resources/Audio") -ExpectedRelativePaths $audioExpectedFiles.ToArray()
Assert-ExactFileSet -Root (Join-Path $root "Resources/Samples") -ExpectedRelativePaths @("ONBOARDING_SAMPLE_LICENSE.md")
Assert-ExactFileSet -Root (Join-Path $root "Resources/Fonts/Inter") -ExpectedRelativePaths @(
    "Inter-Regular.ttf",
    "Inter-SemiBold.ttf",
    "LICENSE.txt",
    "SOURCE.md"
)
$interFontExpectedHashes = [ordered]@{
    "Inter-Regular.ttf"  = "40d692fce188e4471e2b3cba937be967878f631ad3ebbbdcd587687c7ebe0c82"
    "Inter-SemiBold.ttf" = "78a843fade9d4612a5567302fb595b56976eb5fcebf4fea5a5912d638bafcde3"
    "LICENSE.txt"        = "262481e844521b326f5ecd053e59b98c8b2da78c8ee1bdbb6e8174305e54935a"
}
foreach ($fontEntry in $interFontExpectedHashes.GetEnumerator()) {
    $fontPath = Join-Path $root "Resources/Fonts/Inter/$($fontEntry.Key)"
    $fontHash = (Get-FileHash -LiteralPath $fontPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($fontHash -ne $fontEntry.Value) {
        throw "Steam depot Inter 4.1 runtime file hash mismatch: $($fontEntry.Key)"
    }
}
Assert-ExactFileSet -Root (Join-Path $root "Resources/Fonts/SourceSerif4") -ExpectedRelativePaths @(
    "SourceSerif4-Regular.ttf", "LICENSE.md", "SOURCE.md"
)
$atlasFontExpectedHashes = [ordered]@{
    "SourceSerif4-Regular.ttf" = "e5a4ee6a3d87bb9024796be390c6771e2a0eb1883dae25effaf57ca01668e24b"
    "LICENSE.md" = "75784a295293a8992f5a8d99210566e0064a012e6dab6731305e3787f15896c7"
}
foreach ($fontEntry in $atlasFontExpectedHashes.GetEnumerator()) {
    $fontPath = Join-Path $root "Resources/Fonts/SourceSerif4/$($fontEntry.Key)"
    if ((Get-FileHash -LiteralPath $fontPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $fontEntry.Value) {
        throw "Steam depot Source Serif 4.005 runtime file hash mismatch: $($fontEntry.Key)"
    }
}
Assert-ExactFileSet -Root (Join-Path $root "Resources/Maps/Tilesets/Styles") -ExpectedRelativePaths @("fantasy_neutral.json")
. (Join-Path $PSScriptRoot 'portrait-runtime-inventory.ps1')
$portraitRoot = Join-Path $root 'Resources/Portraits'
$portraitFiles = @(Get-PortraitRuntimeInventory -Root $portraitRoot)
Assert-ExactFileSet -Root $portraitRoot -ExpectedRelativePaths $portraitFiles
Assert-ExactFileSet -Root (Join-Path $root "Documentation") -ExpectedRelativePaths @(
    "PRIVACY_DE_EN.md",
    "KNOWN_ISSUES_0_9_DE_EN.md",
    "THIRD_PARTY_NOTICES_DRAFT.md",
    "Licenses/DearImGui-MIT.txt",
    "Licenses/GLFW-zlib.txt",
    "Licenses/SQLiteCpp-MIT.txt"
)

$allFiles = @(Get-ChildItem -LiteralPath $root -Recurse -File -Force)
if ($allFiles.Count -eq 0) {
    throw "Steam depot contains no files: $root"
}

$forbiddenExtensions = @(".db", ".db3", ".sqlite", ".sqlite3", ".bak", ".backup", ".pdb", ".obj", ".lib", ".exp", ".ilk", ".log", ".tmp")
$allowedProvenanceFiles = @("Resources/Fonts/Inter/SOURCE.md", "Resources/Fonts/SourceSerif4/SOURCE.md")
foreach ($file in $allFiles) {
    $relativePath = $file.FullName.Substring($root.Length).TrimStart('\', '/').Replace('\', '/')
    $segments = @($relativePath.Split('/'))
    $forbiddenSegment = $segments | Where-Object {
        $_ -match '^(?i:source|sources|candidate|candidates|backup|backups|recovery)$'
    } | Select-Object -First 1
    if ($null -ne $forbiddenSegment) {
        throw "Steam depot contains forbidden path segment '$forbiddenSegment': $relativePath"
    }
    if ($file.Name -match '(?i)(^|[._-])(source|candidate|backup|recovery)([._-]|$)' -and
        $allowedProvenanceFiles -notcontains $relativePath) {
        throw "Steam depot contains forbidden source/candidate/backup/recovery file: $relativePath"
    }
    if ($forbiddenExtensions -contains $file.Extension.ToLowerInvariant() -or
        $file.Name -match '(?i)\.(db|db3|sqlite|sqlite3)(-wal|-shm|-journal)$') {
        throw "Steam depot contains forbidden database, debug or backup artifact: $relativePath"
    }
}

# Tight directory allowlists prevent future Copy-Item changes from silently adding
# personal campaigns, QA material, prototype art or build output.
Assert-OnlyChildDirectories -Root $root -AllowedNames @("Documentation", "Resources")
Assert-OnlyChildDirectories -Root (Join-Path $root "Resources") -AllowedNames @("Audio", "Samples", "Fonts", "Maps", "Portraits")
Assert-OnlyChildDirectories -Root (Join-Path $root "Resources/Fonts") -AllowedNames @("Inter", "SourceSerif4")
Assert-OnlyChildDirectories -Root (Join-Path $root "Resources/Maps") -AllowedNames @("Tilesets")
Assert-OnlyChildDirectories -Root (Join-Path $root "Resources/Maps/Tilesets") -AllowedNames @("Styles", "Generated")
Assert-OnlyChildDirectories -Root (Join-Path $root "Resources/Maps/Tilesets/Generated") -AllowedNames @("FantasyNeutral")
Assert-OnlyChildDirectories -Root (Join-Path $root "Resources/Maps/Tilesets/Generated/FantasyNeutral") -AllowedNames @("v2", "v3", "v3_1", "v3_2", "v3_3")

$allowedRootFiles = @(
    "GMEditor.exe",
    "gm_editor.install.json",
    "README_DE_EN.txt",
    "VERSION.txt",
    "SHA256SUMS.txt"
)
if ($AllowAppLocalRuntime) {
    $allowedRootFiles += @("msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll", "concrt140.dll")
}
$unexpectedRootFiles = @(Get-ChildItem -LiteralPath $root -File -Force | Where-Object { $allowedRootFiles -notcontains $_.Name })
if ($unexpectedRootFiles.Count -gt 0) {
    throw "Steam depot contains unexpected root file: $($unexpectedRootFiles[0].Name)"
}

$v2RelativeRoot = "Resources/Maps/Tilesets/Generated/FantasyNeutral/v2"
$v2ManifestPath = Join-Path $root "$v2RelativeRoot/packed_runtime_manifest.json"
$v2Manifest = Get-Content -Raw -LiteralPath $v2ManifestPath | ConvertFrom-Json
$v2Assets = @($v2Manifest.assets)
if ($v2Assets.Count -eq 0 -or @($v2Assets | Where-Object { $_.status -ne "accepted" }).Count -gt 0) {
    throw "FantasyNeutral V2 runtime manifest is empty or contains non-accepted assets."
}
if ($null -ne $v2Manifest.PSObject.Properties["asset_count"] -and [int]$v2Manifest.asset_count -ne $v2Assets.Count) {
    throw "FantasyNeutral V2 runtime manifest asset_count does not match its asset list."
}

$expectedV2AtlasPaths = @{}
$v2ExpectedFiles = New-Object System.Collections.Generic.List[string]
$v2ExpectedFiles.Add("packed_runtime_manifest.json")
$v2AssetIds = @{}
$v2Prefix = "Resources/Maps/Tilesets/Generated/FantasyNeutral/v2/atlases/"
foreach ($asset in $v2Assets) {
    $assetId = [string]$asset.id
    if ([string]::IsNullOrWhiteSpace($assetId) -or $v2AssetIds.ContainsKey($assetId)) {
        throw "FantasyNeutral V2 runtime manifest contains a blank or duplicate asset ID '$assetId'."
    }
    $v2AssetIds[$assetId] = $true
    $atlasPath = ([string]$asset.atlas_path).Replace('\', '/')
    if (-not $atlasPath.StartsWith($v2Prefix, [System.StringComparison]::Ordinal) -or
        -not $atlasPath.EndsWith(".png", [System.StringComparison]::OrdinalIgnoreCase) -or
        $atlasPath.Split('/') -contains "..") {
        throw "FantasyNeutral V2 runtime manifest references an unsafe or non-runtime atlas: $atlasPath"
    }
    Assert-DepotFile -Root $root -RelativePath $atlasPath
    $expectedV2AtlasPaths[$atlasPath.ToLowerInvariant()] = $true
    $v2ExpectedFiles.Add($atlasPath.Substring(($v2RelativeRoot + "/").Length))
}

$actualV2AtlasFiles = @(Get-ChildItem -LiteralPath (Join-Path $root "$v2RelativeRoot/atlases") -Recurse -File)
foreach ($atlas in $actualV2AtlasFiles) {
    $relativeAtlas = $atlas.FullName.Substring($root.Length).TrimStart('\', '/').Replace('\', '/')
    if (-not $expectedV2AtlasPaths.ContainsKey($relativeAtlas.ToLowerInvariant())) {
        throw "FantasyNeutral V2 contains an atlas page not referenced by the accepted runtime manifest: $relativeAtlas"
    }
}
if ($actualV2AtlasFiles.Count -ne $expectedV2AtlasPaths.Count) {
    throw "FantasyNeutral V2 runtime atlas count does not match the manifest references."
}
Assert-ExactFileSet -Root (Join-Path $root $v2RelativeRoot) -ExpectedRelativePaths @($v2ExpectedFiles | Sort-Object -Unique)

Assert-ProductionManifest -Root $root -RelativeDirectory "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3"
Assert-ProductionManifest -Root $root -RelativeDirectory "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3_1"
Assert-ProductionManifest -Root $root -RelativeDirectory "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3_2"
Assert-ProductionManifest -Root $root -RelativeDirectory "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3_3"

$v3Snapshot = Get-Content -Raw -LiteralPath (Join-Path $root "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3/production_style_snapshot.json") | ConvertFrom-Json
$v31Snapshot = Get-Content -Raw -LiteralPath (Join-Path $root "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3_1/production_style_snapshot.json") | ConvertFrom-Json
$v32Snapshot = Get-Content -Raw -LiteralPath (Join-Path $root "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3_2/production_style_snapshot.json") | ConvertFrom-Json
$v33Snapshot = Get-Content -Raw -LiteralPath (Join-Path $root "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3_3/production_style_snapshot.json") | ConvertFrom-Json
if ([string]$v31Snapshot.atlas_version -ne "3.1" -or
    [string]$v31Snapshot.base_snapshot_id -ne [string]$v3Snapshot.snapshot_id -or
    [string]$v32Snapshot.atlas_version -ne "3.2" -or
    [string]$v32Snapshot.base_snapshot_id -ne [string]$v31Snapshot.snapshot_id -or
    [string]$v33Snapshot.atlas_version -ne "3.3" -or
    [string]$v33Snapshot.base_snapshot_id -ne [string]$v32Snapshot.snapshot_id) {
    throw "FantasyNeutral V3/V3.1/V3.2/V3.3 production snapshot lineage is inconsistent."
}

$checksumPath = Join-Path $root "SHA256SUMS.txt"
$checksumLines = @(Get-Content -LiteralPath $checksumPath | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
$manifestEntries = @{}
foreach ($line in $checksumLines) {
    if ($line -notmatch '^([0-9a-fA-F]{64})  (.+)$') {
        throw "Invalid SHA256SUMS line: $line"
    }
    $expectedHash = $Matches[1].ToLowerInvariant()
    $relativePath = $Matches[2].Replace('\', '/')
    if ([System.IO.Path]::IsPathRooted($relativePath) -or $relativePath.Split('/') -contains "..") {
        throw "SHA256SUMS contains unsafe path: $relativePath"
    }
    if ($manifestEntries.ContainsKey($relativePath.ToLowerInvariant())) {
        throw "SHA256SUMS contains duplicate path: $relativePath"
    }
    $filePath = Join-Path $root $relativePath
    Assert-DepotFile -Root $root -RelativePath $relativePath
    $actualHash = (Get-FileHash -LiteralPath $filePath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actualHash -ne $expectedHash) {
        throw "SHA-256 mismatch for $relativePath"
    }
    $manifestEntries[$relativePath.ToLowerInvariant()] = $true
}

$filesThatMustBeHashed = @($allFiles | Where-Object { $_.FullName -ne $checksumPath })
foreach ($file in $filesThatMustBeHashed) {
    $relativePath = $file.FullName.Substring($root.Length).TrimStart('\', '/').Replace('\', '/')
    if (-not $manifestEntries.ContainsKey($relativePath.ToLowerInvariant())) {
        throw "SHA256SUMS is missing depot file: $relativePath"
    }
}
if ($manifestEntries.Count -ne $filesThatMustBeHashed.Count) {
    throw "SHA256SUMS entry count does not match the depot file count."
}

$totalBytes = ($allFiles | Measure-Object -Property Length -Sum).Sum
Write-Host "Steam depot validation passed."
Write-Host "Depot: $root"
Write-Host "Files: $($allFiles.Count)"
Write-Host "Bytes: $totalBytes"
Write-Host "FantasyNeutral V2 accepted runtime assets: $($v2Assets.Count)"

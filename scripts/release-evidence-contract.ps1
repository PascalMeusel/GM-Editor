#requires -Version 5.1

Set-StrictMode -Version Latest

function Get-ReleaseEvidenceFileSha256 {
    param([Parameter(Mandatory = $true)][string]$Path)
    $stream = [IO.File]::OpenRead($Path)
    $algorithm = [Security.Cryptography.SHA256]::Create()
    try {
        $bytes = $algorithm.ComputeHash($stream)
        return ([BitConverter]::ToString($bytes)).Replace("-", "").ToLowerInvariant()
    }
    finally {
        $algorithm.Dispose()
        $stream.Dispose()
    }
}

function Assert-ReleaseEvidenceObjectProperties {
    param(
        [Parameter(Mandatory = $true)]$Object,
        [Parameter(Mandatory = $true)][string[]]$Names,
        [Parameter(Mandatory = $true)][string]$EvidenceName
    )
    foreach ($name in $Names) {
        if ($null -eq $Object.PSObject.Properties[$name]) {
            throw "$EvidenceName is missing required property '$name'."
        }
    }
}

function Assert-ReleaseEvidencePathsAreDistinct {
    param(
        [Parameter(Mandatory = $true)][hashtable]$RolePaths,
        [Parameter(Mandatory = $true)][string]$OutputPath
    )

    $output = [IO.Path]::GetFullPath($OutputPath)
    $seen = @{}
    foreach ($entry in $RolePaths.GetEnumerator()) {
        if ([string]::IsNullOrWhiteSpace([string]$entry.Value)) { continue }
        $path = [IO.Path]::GetFullPath([string]$entry.Value)
        $key = $path.ToLowerInvariant()
        if ($key -eq $output.ToLowerInvariant() -or $key -eq ("$output.sha256").ToLowerInvariant()) {
            throw "Evidence '$($entry.Key)' may not reuse the output report or its hash sidecar: $path"
        }
        if ($seen.ContainsKey($key)) {
            throw "Evidence roles '$($seen[$key])' and '$($entry.Key)' resolve to the same file: $path"
        }
        $seen[$key] = [string]$entry.Key
    }
}

function Assert-ReleaseCTestEvidence {
    param(
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][ValidateRange(1, 100000)][int]$ExpectedTestCount
    )

    if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
        throw "CTest log is missing: $LogPath"
    }
    $text = Get-Content -LiteralPath $LogPath -Raw -Encoding UTF8
    $summaries = @([regex]::Matches($text, '(?m)^100% tests passed, 0 tests failed out of (\d+)\s*$'))
    if ($summaries.Count -ne 1) {
        throw "CTest log must contain exactly one zero-failure summary."
    }
    $summaryCount = [int]$summaries[0].Groups[1].Value
    if ($summaryCount -ne $ExpectedTestCount) {
        throw "CTest log proves $summaryCount tests; exactly $ExpectedTestCount are required for this release contract."
    }
    if (@([regex]::Matches($text, '(?m)^Total Test time \(real\)\s*=')).Count -ne 1) {
        throw "CTest log must contain exactly one completed total-time line."
    }
    if ($text -match '(?im)^The following tests FAILED:' -or
        $text -match '(?im)^Errors while running CTest' -or
        $text -match '\*\*\*(?:Failed|Not Run)') {
        throw "CTest log contains a failure or incomplete-test marker."
    }

    $resultMatches = @([regex]::Matches(
        $text,
        '(?m)^\s*(\d+)\/(\d+)\s+Test\s+#\d+:.*?\s+(Passed|Failed|Not Run)\s+[0-9.]+\s+sec\s*$'))
    if ($resultMatches.Count -ne $ExpectedTestCount) {
        throw "CTest log contains $($resultMatches.Count) result rows; exactly $ExpectedTestCount are required."
    }
    $ordinals = New-Object 'System.Collections.Generic.HashSet[int]'
    foreach ($match in $resultMatches) {
        $ordinal = [int]$match.Groups[1].Value
        $denominator = [int]$match.Groups[2].Value
        $status = $match.Groups[3].Value
        if ($denominator -ne $ExpectedTestCount -or $status -cne "Passed" -or
            $ordinal -lt 1 -or $ordinal -gt $ExpectedTestCount -or -not $ordinals.Add($ordinal)) {
            throw "CTest result rows are duplicated, incomplete, failed, or use a different test inventory."
        }
    }
    if ($ordinals.Count -ne $ExpectedTestCount) {
        throw "CTest result ordinals do not cover the complete release inventory."
    }

    return [PSCustomObject][ordered]@{
        testCount = $ExpectedTestCount
        sha256 = Get-ReleaseEvidenceFileSha256 -Path $LogPath
    }
}

function Get-ReleaseProductionSnapshotContract {
    param([Parameter(Mandatory = $true)][string]$SnapshotPath)

    if (-not (Test-Path -LiteralPath $SnapshotPath -PathType Leaf)) {
        throw "Current production snapshot is missing: $SnapshotPath"
    }
    $snapshot = Get-Content -LiteralPath $SnapshotPath -Raw -Encoding UTF8 | ConvertFrom-Json -ErrorAction Stop
    Assert-ReleaseEvidenceObjectProperties -Object $snapshot -EvidenceName "Production snapshot" -Names @(
        "schema_version", "state", "style_id", "style_version", "atlas_version", "snapshot_id",
        "base_snapshot_id", "asset_count", "manifest_sha256", "assets"
    )
    if ([int]$snapshot.schema_version -ne 1 -or [string]$snapshot.state -cne "production" -or
        [string]$snapshot.style_id -cne "fantasy_neutral" -or [int]$snapshot.style_version -ne 3 -or
        [string]$snapshot.atlas_version -cne "3.3" -or
        [string]$snapshot.snapshot_id -notmatch '^fantasy_neutral_v3_3_[0-9A-Za-z]+$' -or
        [string]$snapshot.base_snapshot_id -notmatch '^fantasy_neutral_v3_2_[0-9A-Za-z]+$') {
        throw "Snapshot is not the canonical FantasyNeutral V3.3 production contract."
    }
    if ([string]$snapshot.manifest_sha256 -notmatch '^[0-9a-fA-F]{64}$') {
        throw "Production snapshot has an invalid manifest SHA-256."
    }

    $root = Split-Path -Parent $SnapshotPath
    $manifestPath = Join-Path $root "pack_manifest.json"
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        throw "Production snapshot pack manifest is missing: $manifestPath"
    }
    $manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json -ErrorAction Stop
    Assert-ReleaseEvidenceObjectProperties -Object $manifest -EvidenceName "Production manifest" -Names @(
        "schema_version", "style_id", "style_version", "atlas_version", "snapshot_id",
        "base_snapshot_id", "asset_count", "accepted_count", "assets"
    )
    $manifestHash = Get-ReleaseEvidenceFileSha256 -Path $manifestPath
    if ($manifestHash -ne ([string]$snapshot.manifest_sha256).ToLowerInvariant()) {
        throw "Production snapshot does not match pack_manifest.json."
    }
    if ($null -ne $snapshot.PSObject.Properties["source_manifest_sha256"] -and
        ([string]$snapshot.source_manifest_sha256).ToLowerInvariant() -ne $manifestHash) {
        throw "Production snapshot source_manifest_sha256 does not match pack_manifest.json."
    }
    if ([int]$manifest.schema_version -ne 1 -or
        [string]$manifest.style_id -cne [string]$snapshot.style_id -or
        [int]$manifest.style_version -ne [int]$snapshot.style_version -or
        [string]$manifest.atlas_version -cne [string]$snapshot.atlas_version -or
        [string]$manifest.snapshot_id -cne [string]$snapshot.snapshot_id -or
        [string]$manifest.base_snapshot_id -cne [string]$snapshot.base_snapshot_id) {
        throw "Production manifest identity does not match its V3.3 snapshot."
    }

    $manifestAssets = @($manifest.assets)
    $snapshotAssets = @($snapshot.assets)
    if ($manifestAssets.Count -lt 1 -or
        $manifestAssets.Count -ne [int]$manifest.asset_count -or
        $manifestAssets.Count -ne [int]$manifest.accepted_count -or
        $snapshotAssets.Count -ne [int]$snapshot.asset_count -or
        $snapshotAssets.Count -ne $manifestAssets.Count) {
        throw "Production snapshot asset inventory is empty or inconsistent."
    }
    foreach ($countField in @("candidate_count", "needs_repaint_count")) {
        if ($null -ne $manifest.PSObject.Properties[$countField] -and [int]$manifest.PSObject.Properties[$countField].Value -ne 0) {
            throw "Production manifest reports non-production assets in '$countField'."
        }
    }

    $snapshotById = @{}
    foreach ($asset in $snapshotAssets) {
        $id = [string]$asset.id
        if ([string]::IsNullOrWhiteSpace($id) -or $snapshotById.ContainsKey($id)) {
            throw "Production snapshot contains a blank or duplicate asset ID '$id'."
        }
        $snapshotById[$id] = $asset
    }
    $rootPath = [IO.Path]::GetFullPath($root).TrimEnd('\', '/')
    $expectedFiles = @{
        "pack_manifest.json" = $true
        "production_style_snapshot.json" = $true
    }
    $manifestIds = @{}
    foreach ($asset in $manifestAssets) {
        $id = [string]$asset.id
        $relativeFile = ([string]$asset.file).Replace('\', '/')
        if ([string]::IsNullOrWhiteSpace($id) -or $manifestIds.ContainsKey($id) -or
            [string]::IsNullOrWhiteSpace($relativeFile) -or [IO.Path]::IsPathRooted($relativeFile) -or
            $relativeFile.Split('/') -contains "..") {
            throw "Production manifest contains a blank, duplicate, or unsafe asset entry '$id'."
        }
        if ([string]$asset.status -cne "accepted" -or -not $snapshotById.ContainsKey($id)) {
            throw "Production manifest contains a non-accepted or unbound asset '$id'."
        }
        $snapshotAsset = $snapshotById[$id]
        if ([string]$snapshotAsset.status -cne "accepted" -or
            ([string]$snapshotAsset.file).Replace('\', '/') -cne $relativeFile -or
            ([string]$snapshotAsset.sha256).ToLowerInvariant() -cne ([string]$asset.sha256).ToLowerInvariant()) {
            throw "Production snapshot asset '$id' differs from its accepted manifest record."
        }
        $assetPath = [IO.Path]::GetFullPath((Join-Path $rootPath $relativeFile))
        if (-not $assetPath.StartsWith($rootPath + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
            -not (Test-Path -LiteralPath $assetPath -PathType Leaf)) {
            throw "Production manifest asset is missing or escapes its snapshot directory: $relativeFile"
        }
        $declaredHash = ([string]$asset.sha256).ToLowerInvariant()
        if ($declaredHash -notmatch '^[0-9a-f]{64}$' -or
            (Get-ReleaseEvidenceFileSha256 -Path $assetPath) -cne $declaredHash) {
            throw "Production asset hash mismatch for '$id'."
        }
        $manifestIds[$id] = $true
        $fileKey = $relativeFile.ToLowerInvariant()
        if ($expectedFiles.ContainsKey($fileKey)) {
            throw "Production manifest repeats asset file '$relativeFile'."
        }
        $expectedFiles[$fileKey] = $true
    }
    foreach ($file in @(Get-ChildItem -LiteralPath $rootPath -Recurse -File -Force)) {
        $relativeFile = $file.FullName.Substring($rootPath.Length).TrimStart('\', '/').Replace('\', '/')
        if (-not $expectedFiles.ContainsKey($relativeFile.ToLowerInvariant())) {
            throw "Production snapshot directory contains an unmanifested file: $relativeFile"
        }
    }
    if (@(Get-ChildItem -LiteralPath $rootPath -Recurse -File -Force).Count -ne $expectedFiles.Count) {
        throw "Production snapshot directory does not exactly match its accepted manifest inventory."
    }

    $releaseEligibleProperty = $snapshot.PSObject.Properties["release_eligible"]
    $rightsProperty = $snapshot.PSObject.Properties["distribution"]
    $releaseEligible = $null -ne $releaseEligibleProperty -and
        $releaseEligibleProperty.Value -is [bool] -and [bool]$releaseEligibleProperty.Value
    $rightsClear = $null -ne $rightsProperty -and $null -ne $snapshot.distribution -and
        $null -ne $snapshot.distribution.PSObject.Properties["rights_clearance_complete"] -and
        $snapshot.distribution.rights_clearance_complete -is [bool] -and
        [bool]$snapshot.distribution.rights_clearance_complete

    return [PSCustomObject][ordered]@{
        path = [IO.Path]::GetFullPath($SnapshotPath)
        sha256 = Get-ReleaseEvidenceFileSha256 -Path $SnapshotPath
        manifestPath = [IO.Path]::GetFullPath($manifestPath)
        manifestSha256 = $manifestHash
        snapshotId = [string]$snapshot.snapshot_id
        styleId = [string]$snapshot.style_id
        styleVersion = [int]$snapshot.style_version
        atlasVersion = [string]$snapshot.atlas_version
        baseSnapshotId = [string]$snapshot.base_snapshot_id
        assetCount = $manifestAssets.Count
        releaseEligible = $releaseEligible
        rightsClearanceComplete = $rightsClear
    }
}

function Assert-ReleaseMapQaEvidence {
    param(
        [Parameter(Mandatory = $true)]$Report,
        [Parameter(Mandatory = $true)]$SnapshotContract,
        [ValidateRange(1, 1000000)][int]$ExpectedMapCount = 50000,
        [ValidateRange(1, 256)][int]$ExpectedShardCount = 4
    )

    Assert-ReleaseEvidenceObjectProperties -Object $Report -EvidenceName "Map QA report" -Names @(
        "schema_version", "product_contract", "generator_version", "recipe_version", "style_id",
        "style_version", "style_snapshot_id", "tier", "requested_map_count", "requested_shard_map_count",
        "processed_map_count", "shard_count", "quality_attempts", "determinism_checks",
        "determinism_failures", "serialization_failures", "failure_count", "diversity_passed",
        "performance_passed", "passed", "groups", "failure_signatures"
    )
    if ([int]$Report.schema_version -ne 2 -or
        [string]$Report.product_contract -cne "MapCreationRequest/MapGenerationBatchResult" -or
        [int]$Report.generator_version -ne 5 -or [int]$Report.recipe_version -ne 3) {
        throw "Map QA is not bound to the schema-V2 V5 product candidate contract."
    }
    if ([string]$Report.style_snapshot_id -cne [string]$SnapshotContract.snapshotId -or
        [string]$Report.style_id -cne [string]$SnapshotContract.styleId -or
        [int]$Report.style_version -ne [int]$SnapshotContract.styleVersion) {
        throw "Map QA is not bound to the current FantasyNeutral production snapshot."
    }
    if ([string]$Report.tier -cne "release" -or
        [int64]$Report.requested_map_count -ne $ExpectedMapCount -or
        [int64]$Report.requested_shard_map_count -ne $ExpectedMapCount -or
        [int64]$Report.processed_map_count -ne $ExpectedMapCount -or
        [int]$Report.shard_count -ne $ExpectedShardCount -or
        [int]$Report.quality_attempts -ne 48 -or
        [int64]$Report.determinism_checks -ne $ExpectedMapCount) {
        throw "Map QA does not represent the exact release tier, workload, shard inventory, repair budget, and determinism coverage."
    }
    foreach ($booleanName in @("passed", "diversity_passed", "performance_passed")) {
        $value = $Report.PSObject.Properties[$booleanName].Value
        if ($value -isnot [bool] -or -not [bool]$value) {
            throw "Map QA boolean gate '$booleanName' is not exactly true."
        }
    }
    if ([int64]$Report.failure_count -ne 0 -or [int64]$Report.determinism_failures -ne 0 -or
        [int64]$Report.serialization_failures -ne 0 -or @($Report.failure_signatures).Count -ne 0) {
        throw "Map QA contains failures or regression signatures."
    }

    $requiredGroups = @("world", "region_travel", "city_settlement", "tactical_outdoor", "dungeon_cave")
    if (@($Report.groups).Count -ne $requiredGroups.Count) {
        throw "Map QA must contain exactly the five V5 map groups."
    }
    foreach ($groupName in $requiredGroups) {
        $group = @($Report.groups | Where-Object { [string]$_.name -ceq $groupName })
        if ($group.Count -ne 1 -or [int64]$group[0].count -ne ($ExpectedMapCount / 5) -or
            [int64]$group[0].failures -ne 0 -or [int64]$group[0].p95_us -le 0) {
            throw "Map QA group '$groupName' is missing, duplicated, incomplete, failed, or lacks timing evidence."
        }
        $minimumDiversity = if ($groupName -in @("tactical_outdoor", "dungeon_cave")) { 0.95 } else { 0.99 }
        if ([double]$group[0].unique_ratio -lt $minimumDiversity) {
            throw "Map QA group '$groupName' misses its diversity threshold."
        }
    }

    return [PSCustomObject][ordered]@{
        mapCount = $ExpectedMapCount
        shardCount = $ExpectedShardCount
        snapshotId = [string]$SnapshotContract.snapshotId
    }
}

param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$OutputDirectory = "",
    [string]$IdContains = "material_retry",
    [int]$StartVariant = 113,
    [int]$EndVariant = 136,
    [string[]]$Families = @("grass_open", "snow", "water_shallow", "dirt"),
    [int]$TileSize = 40,
    [int]$Width = 16,
    [int]$Height = 16,
    [int]$EdgeBand = 2,
    [double]$WarningThreshold = 18.0
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

$resolvedSourceRoot = Resolve-TilesetPath $SourceRoot
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $resolvedSourceRoot "review/base_batch_reviews/$IdContains`_v$($StartVariant.ToString("000"))_v$($EndVariant.ToString("000"))"
}
$resolvedOutputDirectory = Resolve-TilesetPath $OutputDirectory
New-Item -ItemType Directory -Force -Path $resolvedOutputDirectory | Out-Null

$bundle = Get-TilesetV2ManifestBundle $resolvedSourceRoot
$tileAssets = @(Get-TilesetManifestAssets $bundle.Tiles | Where-Object {
    (Get-TilesetProperty $_ "asset_class" "") -eq "base" -and
    (Get-TilesetProperty $_ "status" "") -in @("candidate", "accepted") -and
    [int](Get-TilesetProperty $_ "variant" 0) -ge $StartVariant -and
    [int](Get-TilesetProperty $_ "variant" 0) -le $EndVariant -and
    (Get-TilesetProperty $_ "id" "") -like "*$IdContains*" -and
    -not [string]::IsNullOrWhiteSpace((Get-TilesetProperty $_ "source_path" ""))
})

$familyReports = @()
$allWarnings = @()

foreach ($family in $Families) {
    $familyAssets = @($tileAssets | Where-Object {
        (Get-TilesetProperty $_ "owner" "") -eq $family
    } | Sort-Object { [int](Get-TilesetProperty $_ "variant" 0) })

    if ($familyAssets.Count -eq 0) {
        $familyReports += [pscustomobject]@{
            family = $family
            status = "skipped"
            reason = "no_matching_assets"
            asset_count = 0
        }
        continue
    }

    $repeatPath = Join-Path $resolvedOutputDirectory "$family`__$IdContains`_v$($StartVariant.ToString("000"))_v$($EndVariant.ToString("000"))__repeat.png"
    $review = New-TilesetReviewBitmap ($Width * $TileSize) ($Height * $TileSize)
    try {
        for ($y = 0; $y -lt $Height; ++$y) {
            for ($x = 0; $x -lt $Width; ++$x) {
                $asset = $familyAssets[(Get-TilesetStableIndex $x $y 113 $familyAssets.Count)]
                $dest = [Drawing.Rectangle]::new($x * $TileSize, $y * $TileSize, $TileSize, $TileSize)
                Draw-TilesetAsset $review.Graphics $asset $dest
            }
        }
        $review.Bitmap.Save($repeatPath, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $review.Graphics.Dispose()
        $review.Bitmap.Dispose()
    }

    $pairReports = @()
    $selfWrapReports = @()
    $edgeCache = @{}
    for ($i = 0; $i -lt $familyAssets.Count; ++$i) {
        $asset = $familyAssets[$i]
        $assetPath = Get-TilesetAssetSourcePath $asset
        $assetId = Get-TilesetProperty $asset "id" ""
        if (-not (Test-Path -LiteralPath $assetPath)) {
            $allWarnings += [pscustomobject]@{
                family = $family
                id = $assetId
                issue = "missing_source_file"
            }
            continue
        }

        $edgeCache[$assetId] = [pscustomobject]@{
            north = Get-TilesetEdgeAverage $assetPath "north" $EdgeBand
            east = Get-TilesetEdgeAverage $assetPath "east" $EdgeBand
            south = Get-TilesetEdgeAverage $assetPath "south" $EdgeBand
            west = Get-TilesetEdgeAverage $assetPath "west" $EdgeBand
        }
    }

    for ($i = 0; $i -lt $familyAssets.Count; ++$i) {
        $asset = $familyAssets[$i]
        $assetId = Get-TilesetProperty $asset "id" ""
        if (-not $edgeCache.ContainsKey($assetId)) {
            continue
        }

        $assetEdges = $edgeCache[$assetId]
        $selfEastWest = Get-TilesetColorDistance $assetEdges.east $assetEdges.west
        $selfNorthSouth = Get-TilesetColorDistance $assetEdges.north $assetEdges.south
        $selfMax = @($selfEastWest, $selfNorthSouth) | Measure-Object -Maximum | Select-Object -ExpandProperty Maximum
        $selfReport = [pscustomobject]@{
            id = $assetId
            east_to_west_delta = [Math]::Round($selfEastWest, 2)
            north_to_south_delta = [Math]::Round($selfNorthSouth, 2)
            max_delta = [Math]::Round($selfMax, 2)
            status = if ($selfMax -gt $WarningThreshold) { "warn" } else { "ok" }
        }
        $selfWrapReports += $selfReport
        if ($selfMax -gt $WarningThreshold) {
            $allWarnings += [pscustomobject]@{
                family = $family
                id = $selfReport.id
                max_delta = $selfReport.max_delta
                issue = "self_wrap_edge_delta"
            }
        }

        for ($j = $i + 1; $j -lt $familyAssets.Count; ++$j) {
            $left = $familyAssets[$i]
            $right = $familyAssets[$j]
            $leftId = Get-TilesetProperty $left "id" ""
            $rightId = Get-TilesetProperty $right "id" ""
            if (-not $edgeCache.ContainsKey($leftId) -or -not $edgeCache.ContainsKey($rightId)) {
                continue
            }
            $leftEdges = $edgeCache[$leftId]
            $rightEdges = $edgeCache[$rightId]

            $eastWestDelta = Get-TilesetColorDistance $leftEdges.east $rightEdges.west
            $westEastDelta = Get-TilesetColorDistance $leftEdges.west $rightEdges.east
            $northSouthDelta = Get-TilesetColorDistance $leftEdges.north $rightEdges.south
            $southNorthDelta = Get-TilesetColorDistance $leftEdges.south $rightEdges.north
            $maxDelta = @($eastWestDelta, $westEastDelta, $northSouthDelta, $southNorthDelta) | Measure-Object -Maximum | Select-Object -ExpandProperty Maximum
            $pairReport = [pscustomobject]@{
                left_id = $leftId
                right_id = $rightId
                east_to_west_delta = [Math]::Round($eastWestDelta, 2)
                west_to_east_delta = [Math]::Round($westEastDelta, 2)
                north_to_south_delta = [Math]::Round($northSouthDelta, 2)
                south_to_north_delta = [Math]::Round($southNorthDelta, 2)
                max_delta = [Math]::Round($maxDelta, 2)
                status = if ($maxDelta -gt $WarningThreshold) { "warn" } else { "ok" }
            }
            $pairReports += $pairReport
            if ($maxDelta -gt $WarningThreshold) {
                $allWarnings += [pscustomobject]@{
                    family = $family
                    left_id = $pairReport.left_id
                    right_id = $pairReport.right_id
                    max_delta = $pairReport.max_delta
                    issue = "same_family_cross_variant_edge_delta"
                }
            }
        }
    }

    $familyWarnings = @($allWarnings | Where-Object { $_.family -eq $family })
    $maxPairDelta = 0.0
    if ($pairReports.Count -gt 0) {
        $maxPairDelta = ($pairReports | Measure-Object max_delta -Maximum).Maximum
    }
    $maxSelfDelta = 0.0
    if ($selfWrapReports.Count -gt 0) {
        $maxSelfDelta = ($selfWrapReports | Measure-Object max_delta -Maximum).Maximum
    }

    $familyReports += [pscustomobject]@{
        family = $family
        status = "generated"
        asset_count = $familyAssets.Count
        repeat_sheet = ConvertTo-TilesetRelativePath $repeatPath
        pair_check_count = $pairReports.Count
        self_wrap_check_count = $selfWrapReports.Count
        warning_count = $familyWarnings.Count
        max_cross_variant_delta = [Math]::Round($maxPairDelta, 2)
        max_self_wrap_delta = [Math]::Round($maxSelfDelta, 2)
        self_wrap_checks = $selfWrapReports
        pair_checks = $pairReports
    }
}

$reportPath = Join-Path $resolvedOutputDirectory "base_batch_review.json"
$report = [pscustomobject]@{
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    source_root = ConvertTo-TilesetRelativePath $resolvedSourceRoot
    id_contains = $IdContains
    start_variant = $StartVariant
    end_variant = $EndVariant
    tile_size = $TileSize
    repeat_width = $Width
    repeat_height = $Height
    edge_band = $EdgeBand
    warning_threshold = $WarningThreshold
    summary = [pscustomobject]@{
        asset_count = $tileAssets.Count
        family_count = $familyReports.Count
        warning_count = $allWarnings.Count
    }
    warnings = $allWarnings
    families = $familyReports
}
$report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $reportPath

Write-Host "Base batch review wrote $reportPath"
Write-Host "Assets: $($tileAssets.Count); warnings: $($allWarnings.Count)"

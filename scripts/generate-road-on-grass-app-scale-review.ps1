param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$OutputDirectory = "",
    [string]$WorkOrderId = "fn_v2_way_dirt_road_on_grass_open",
    [string]$GroundOwner = "grass_open",
    [string]$WayOwner = "way_dirt_road",
    [int[]]$TileSizes = @(24, 32, 48)
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

$resolvedSourceRoot = Resolve-TilesetPath $SourceRoot
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $resolvedSourceRoot "review/contextual_candidates"
}
$resolvedOutputDirectory = Resolve-TilesetPath $OutputDirectory
New-Item -ItemType Directory -Force -Path $resolvedOutputDirectory | Out-Null

function Write-CleanJsonFile([string]$Path, $Value) {
    $json = $Value | ConvertTo-Json -Depth 16
    if ($json -is [array]) {
        $json = $json -join "`n"
    }
    $json = $json -replace "`r`n", "`n"
    $json = $json -replace "`r", "`n"
    $json = (($json -split "`n") | ForEach-Object { $_.TrimEnd() }) -join "`n"
    [System.IO.File]::WriteAllText($Path, $json.TrimEnd() + "`n", [System.Text.UTF8Encoding]::new($false))
}

function ConvertFrom-MaskToken([string]$MaskToken) {
    if ($MaskToken -match "^mask([0-9]{3})$") {
        return [int]$Matches[1]
    }
    return $null
}

function Format-MaskToken([int]$Mask) {
    return "mask$($Mask.ToString("000"))"
}

function New-BoolGrid([int]$Width, [int]$Height) {
    $grid = @()
    for ($y = 0; $y -lt $Height; ++$y) {
        $row = @()
        for ($x = 0; $x -lt $Width; ++$x) {
            $row += $false
        }
        $grid += ,$row
    }
    return $grid
}

function Set-RoadCell($Grid, [int]$X, [int]$Y) {
    if ($Y -ge 0 -and $Y -lt $Grid.Count -and $X -ge 0 -and $X -lt $Grid[$Y].Count) {
        $Grid[$Y][$X] = $true
    }
}

function Test-RoadCell($Grid, [int]$X, [int]$Y) {
    return $Y -ge 0 -and $Y -lt $Grid.Count -and $X -ge 0 -and $X -lt $Grid[$Y].Count -and $Grid[$Y][$X]
}

function Get-RoadMaskForGrid($Grid, [int]$X, [int]$Y) {
    $mask = 0
    if (Test-RoadCell $Grid $X ($Y - 1)) { $mask = $mask -bor 1 }
    if (Test-RoadCell $Grid ($X + 1) ($Y - 1)) { $mask = $mask -bor 2 }
    if (Test-RoadCell $Grid ($X + 1) $Y) { $mask = $mask -bor 4 }
    if (Test-RoadCell $Grid ($X + 1) ($Y + 1)) { $mask = $mask -bor 8 }
    if (Test-RoadCell $Grid $X ($Y + 1)) { $mask = $mask -bor 16 }
    if (Test-RoadCell $Grid ($X - 1) ($Y + 1)) { $mask = $mask -bor 32 }
    if (Test-RoadCell $Grid ($X - 1) $Y) { $mask = $mask -bor 64 }
    if (Test-RoadCell $Grid ($X - 1) ($Y - 1)) { $mask = $mask -bor 128 }
    return $mask
}

function Add-RoadCells($Grid, [object[]]$Cells) {
    foreach ($cell in $Cells) {
        Set-RoadCell $Grid ([int]$cell[0]) ([int]$cell[1])
    }
}

function Select-GrassAsset([int]$X, [int]$Y) {
    return Select-TilesetAsset $script:tileAssets $GroundOwner @("base") $null $X $Y 811
}

function Select-RoadAsset([int]$Mask, [int]$X, [int]$Y, [int]$Salt = 0) {
    $pool = @($script:roadAssets | Where-Object { [int](Get-TilesetProperty $_ "mask" -1) -eq $Mask } | Sort-Object { Get-TilesetProperty $_ "id" "" })
    if ($pool.Count -eq 0) {
        return $null
    }
    return $pool[(Get-TilesetStableIndex $X $Y (1321 + $Salt) $pool.Count)]
}

function Draw-Label($Graphics, [string]$Text, [int]$X, [int]$Y, [int]$Width = 260) {
    $font = [Drawing.Font]::new("Segoe UI", 9)
    $brush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(235, 238, 242, 246))
    $backBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(168, 8, 12, 16))
    try {
        $Graphics.FillRectangle($backBrush, $X, $Y, $Width, 18)
        $Graphics.DrawString($Text, $font, $brush, [Drawing.PointF]::new($X + 4, $Y + 2))
    }
    finally {
        $font.Dispose()
        $brush.Dispose()
        $backBrush.Dispose()
    }
}

function Draw-MissingRoadCell($Graphics, [Drawing.Rectangle]$Destination, [int]$Mask) {
    $brush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(150, 120, 24, 42))
    $pen = [Drawing.Pen]::new([Drawing.Color]::FromArgb(230, 255, 212, 92), 2)
    try {
        $Graphics.FillRectangle($brush, $Destination)
        $Graphics.DrawRectangle($pen, $Destination.X, $Destination.Y, $Destination.Width - 1, $Destination.Height - 1)
        Draw-Label $Graphics (Format-MaskToken $Mask) $Destination.X ($Destination.Y + [Math]::Max(2, [int]($Destination.Height / 3))) 88
    }
    finally {
        $brush.Dispose()
        $pen.Dispose()
    }
}

function Get-Luminance([Drawing.Color]$Color) {
    return (0.2126 * $Color.R) + (0.7152 * $Color.G) + (0.0722 * $Color.B)
}

function Get-AssetContrastMetrics($RoadAsset, $GrassAsset) {
    $roadPath = Get-TilesetAssetSourcePath $RoadAsset
    $grassPath = Get-TilesetAssetSourcePath $GrassAsset
    $road = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $roadPath).Path)
    $grass = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $grassPath).Path)
    try {
        $roadPixels = 0
        $sumLumaDelta = 0.0
        $sumRgbDistance = 0.0
        $sumAlpha = 0.0
        for ($y = 0; $y -lt $road.Height; ++$y) {
            for ($x = 0; $x -lt $road.Width; ++$x) {
                $rp = $road.GetPixel($x, $y)
                if ($rp.A -lt 48) {
                    continue
                }
                $gp = $grass.GetPixel($x % $grass.Width, $y % $grass.Height)
                $alpha = $rp.A / 255.0
                $comp = [Drawing.Color]::FromArgb(
                    255,
                    [int](($rp.R * $alpha) + ($gp.R * (1.0 - $alpha))),
                    [int](($rp.G * $alpha) + ($gp.G * (1.0 - $alpha))),
                    [int](($rp.B * $alpha) + ($gp.B * (1.0 - $alpha)))
                )
                $lumaDelta = [Math]::Abs((Get-Luminance $comp) - (Get-Luminance $gp))
                $rgbDistance = [Math]::Sqrt(
                    ([Math]::Pow(([double]$comp.R - $gp.R), 2.0)) +
                    ([Math]::Pow(([double]$comp.G - $gp.G), 2.0)) +
                    ([Math]::Pow(([double]$comp.B - $gp.B), 2.0))
                )
                ++$roadPixels
                $sumLumaDelta += $lumaDelta
                $sumRgbDistance += $rgbDistance
                $sumAlpha += $rp.A
            }
        }
        if ($roadPixels -eq 0) {
            return [pscustomobject]@{
                road_pixel_count = 0
                average_luma_delta = 0.0
                average_rgb_distance = 0.0
                average_alpha = 0.0
            }
        }
        return [pscustomobject]@{
            road_pixel_count = $roadPixels
            average_luma_delta = [Math]::Round($sumLumaDelta / $roadPixels, 2)
            average_rgb_distance = [Math]::Round($sumRgbDistance / $roadPixels, 2)
            average_alpha = [Math]::Round($sumAlpha / $roadPixels, 2)
        }
    }
    finally {
        $road.Dispose()
        $grass.Dispose()
    }
}

$bundle = Get-TilesetV2ManifestBundle $resolvedSourceRoot
$script:tileAssets = @(Get-TilesetManifestAssets $bundle.Tiles)
$wayAssets = @(Get-TilesetManifestAssets $bundle.Ways)
$script:roadAssets = @($wayAssets | Where-Object {
    (Get-TilesetProperty $_ "work_order_id" "") -eq $WorkOrderId -and
    (Get-TilesetProperty $_ "asset_class" "") -eq "way_segment" -and
    (Get-TilesetProperty $_ "owner" "") -eq $WayOwner -and
    (Get-TilesetProperty $_ "status" "") -in @("candidate", "accepted")
})
$activeVariants = @($script:roadAssets | ForEach-Object { [int](Get-TilesetProperty $_ "variant" 0) } | Sort-Object -Unique)
$variantLabel = if ($activeVariants.Count -gt 0) {
    "v$($activeVariants[0].ToString("000"))_v$($activeVariants[$activeVariants.Count - 1].ToString("000"))"
}
else {
    "no_active_variants"
}

$workOrder = $null
foreach ($candidate in (ConvertTo-TilesetArray (Get-TilesetProperty $bundle.Ways "production_work_orders" @()))) {
    if ((Get-TilesetProperty $candidate "id" "") -eq $WorkOrderId) {
        $workOrder = $candidate
        break
    }
}
if ($null -eq $workOrder) {
    throw "Could not find work order '$WorkOrderId' in ways.json."
}

$requiredMasks = @(
    ConvertTo-TilesetArray (Get-TilesetProperty $workOrder "required_masks_or_roles" @()) |
        ForEach-Object { ConvertFrom-MaskToken ([string]$_) } |
        Where-Object { $null -ne $_ } |
        Sort-Object
)

$routeWidth = 28
$routeHeight = 14
$roadGrid = New-BoolGrid $routeWidth $routeHeight
Add-RoadCells $roadGrid @(@(1,2), @(2,2), @(3,2), @(4,2), @(5,2), @(6,2), @(7,2), @(8,2), @(9,2))
Add-RoadCells $roadGrid @(@(13,1), @(13,2), @(13,3), @(13,4), @(13,5), @(13,6), @(13,7))
Add-RoadCells $roadGrid @(@(2,6), @(3,7), @(4,8), @(5,9), @(6,10), @(7,11))
Add-RoadCells $roadGrid @(@(25,6), @(24,7), @(23,8), @(22,9), @(21,10), @(20,11))
Add-RoadCells $roadGrid @(@(16,2), @(17,2), @(18,2), @(19,2), @(20,2), @(20,3), @(20,4), @(20,5), @(20,6))
Add-RoadCells $roadGrid @(@(10,9), @(10,10), @(10,11), @(10,12), @(11,12), @(12,12), @(13,12))

$sortedTileSizes = @($TileSizes | Sort-Object -Unique)
$maxTileSize = ($sortedTileSizes | Measure-Object -Maximum).Maximum
$sectionGap = 28
$sectionHeights = @($sortedTileSizes | ForEach-Object { ($_ * $routeHeight) + 30 })
$footerHeight = 86
$reviewWidth = [Math]::Max(($routeWidth * $maxTileSize), 720)
$reviewHeight = (($sectionHeights | Measure-Object -Sum).Sum) + (($sortedTileSizes.Count - 1) * $sectionGap) + $footerHeight
$reviewPath = Join-Path $resolvedOutputDirectory "way_dirt_road_on_grass_open_app_scale_review_$variantLabel.png"
$review = New-TilesetReviewBitmap $reviewWidth $reviewHeight
$missingRouteMasks = @{}
$routeMaskUsage = @{}
try {
    $sectionY = 0
    foreach ($tileSize in $sortedTileSizes) {
        Draw-Label $review.Graphics "app-scale road review at $tileSize px cells" 8 ($sectionY + 6) 300
        for ($y = 0; $y -lt $routeHeight; ++$y) {
            for ($x = 0; $x -lt $routeWidth; ++$x) {
                $dest = [Drawing.Rectangle]::new($x * $tileSize, $sectionY + 30 + ($y * $tileSize), $tileSize, $tileSize)
                Draw-TilesetAsset $review.Graphics (Select-GrassAsset $x $y) $dest
            }
        }

        for ($y = 0; $y -lt $routeHeight; ++$y) {
            for ($x = 0; $x -lt $routeWidth; ++$x) {
                if (-not (Test-RoadCell $roadGrid $x $y)) {
                    continue
                }
                $mask = Get-RoadMaskForGrid $roadGrid $x $y
                $maskToken = Format-MaskToken $mask
                if (-not $routeMaskUsage.ContainsKey($maskToken)) { $routeMaskUsage[$maskToken] = 0 }
                $routeMaskUsage[$maskToken] += 1
                $dest = [Drawing.Rectangle]::new($x * $tileSize, $sectionY + 30 + ($y * $tileSize), $tileSize, $tileSize)
                $roadAsset = Select-RoadAsset $mask $x $y $tileSize
                if ($null -eq $roadAsset) {
                    Draw-MissingRoadCell $review.Graphics $dest $mask
                    if (-not $missingRouteMasks.ContainsKey($maskToken)) { $missingRouteMasks[$maskToken] = 0 }
                    $missingRouteMasks[$maskToken] += 1
                }
                else {
                    Draw-TilesetAsset $review.Graphics $roadAsset $dest
                }
            }
        }

        $sectionY += ($tileSize * $routeHeight) + 30 + $sectionGap
    }

    $footerY = $reviewHeight - $footerHeight
    Draw-Label $review.Graphics "active mask255 repaint at app scales" 8 ($footerY + 8) 280
    $mask255Assets = @($script:roadAssets | Where-Object { [int](Get-TilesetProperty $_ "mask" -1) -eq 255 } | Sort-Object { [int](Get-TilesetProperty $_ "variant" 0) })
    $swatchX = 8
    foreach ($tileSize in $sortedTileSizes) {
        for ($index = 0; $index -lt $mask255Assets.Count; ++$index) {
            $dest = [Drawing.Rectangle]::new($swatchX, $footerY + 34, $tileSize, $tileSize)
            Draw-TilesetAsset $review.Graphics (Select-GrassAsset $index (200 + $tileSize)) $dest
            Draw-TilesetAsset $review.Graphics $mask255Assets[$index] $dest
            $swatchX += $tileSize + 5
        }
        $swatchX += 14
    }

    $review.Bitmap.Save($reviewPath, [Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $review.Graphics.Dispose()
    $review.Bitmap.Dispose()
}

$grassForMetrics = Select-GrassAsset 0 0
$contrastRecords = @()
foreach ($roadAsset in ($script:roadAssets | Sort-Object { [int](Get-TilesetProperty $_ "mask" -1) }, { [int](Get-TilesetProperty $_ "variant" 0) })) {
    $metrics = Get-AssetContrastMetrics $roadAsset $grassForMetrics
    $contrastRecords += [pscustomobject]@{
        id = Get-TilesetProperty $roadAsset "id" ""
        mask = Format-MaskToken ([int](Get-TilesetProperty $roadAsset "mask" -1))
        variant = [int](Get-TilesetProperty $roadAsset "variant" 0)
        road_pixel_count = $metrics.road_pixel_count
        average_luma_delta = $metrics.average_luma_delta
        average_rgb_distance = $metrics.average_rgb_distance
        average_alpha = $metrics.average_alpha
    }
}

$lumaStats = $contrastRecords | Measure-Object average_luma_delta -Average -Minimum -Maximum
$rgbStats = $contrastRecords | Measure-Object average_rgb_distance -Average -Minimum -Maximum
$maskStats = @(
    $contrastRecords |
        Group-Object mask |
        Sort-Object Name |
        ForEach-Object {
            $luma = $_.Group | Measure-Object average_luma_delta -Average -Minimum
            $rgb = $_.Group | Measure-Object average_rgb_distance -Average -Minimum
            [pscustomobject]@{
                mask = $_.Name
                active_count = $_.Count
                min_luma_delta = [Math]::Round($luma.Minimum, 2)
                average_luma_delta = [Math]::Round($luma.Average, 2)
                min_rgb_distance = [Math]::Round($rgb.Minimum, 2)
                average_rgb_distance = [Math]::Round($rgb.Average, 2)
            }
        }
)

$jsonPath = Join-Path $resolvedOutputDirectory "way_dirt_road_on_grass_open_app_scale_review_$variantLabel.json"
$report = [pscustomobject]@{
    schema_version = 1
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    source_root = ConvertTo-TilesetRelativePath $resolvedSourceRoot
    work_order_id = $WorkOrderId
    active_variant_label = $variantLabel
    active_variants = $activeVariants
    output_png = ConvertTo-TilesetRelativePath $reviewPath
    tile_sizes = $sortedTileSizes
    active_candidate_count = $script:roadAssets.Count
    required_mask_count = $requiredMasks.Count
    route_mask_usage = @(
        $routeMaskUsage.Keys |
            Sort-Object |
            ForEach-Object {
                [pscustomobject]@{
                    mask = $_
                    draw_count = $routeMaskUsage[$_]
                }
            }
    )
    missing_route_masks = @($missingRouteMasks.Keys | Sort-Object)
    contrast_summary = [pscustomobject]@{
        metric_background_asset = Get-TilesetProperty $grassForMetrics "id" ""
        active_asset_count = $contrastRecords.Count
        average_luma_delta = [Math]::Round($lumaStats.Average, 2)
        min_luma_delta = [Math]::Round($lumaStats.Minimum, 2)
        max_luma_delta = [Math]::Round($lumaStats.Maximum, 2)
        average_rgb_distance = [Math]::Round($rgbStats.Average, 2)
        min_rgb_distance = [Math]::Round($rgbStats.Minimum, 2)
        max_rgb_distance = [Math]::Round($rgbStats.Maximum, 2)
        suggested_min_luma_delta_for_acceptance_review = 18.0
        suggested_min_rgb_distance_for_acceptance_review = 35.0
    }
    contrast_by_mask = $maskStats
    lowest_contrast_assets = @(
        $contrastRecords |
            Sort-Object average_luma_delta, average_rgb_distance |
            Select-Object -First 12
    )
    acceptance_review = [pscustomobject]@{
        status = "candidate_hold_for_acceptance"
        accepted = $false
        app_scale_review_generated = $true
        issue_labels = @(
            "candidate_only",
            "acceptance_hold",
            "app_scale_contrast_review_generated",
            "app_scale_contrast_needs_human_decision",
            "mask255_repaint_needs_app_scale_review"
        )
        notes = @(
            "This review renders the full active road-on-grass route set at multiple app-scale tile sizes and records per-asset contrast metrics against active grass base art.",
            "It does not accept the work order. Promotion still requires a human/art-direction decision that the active road contrast and mask255 all-exit repaint read correctly at target app scale."
        )
    }
}
Write-CleanJsonFile $jsonPath $report

Write-Host "Generated road-on-grass app-scale review:"
Write-Host "  Review sheet: $reviewPath"
Write-Host "  Report:       $jsonPath"
Write-Host "  Active assets: $($script:roadAssets.Count)"
Write-Host "  Average luma delta: $([Math]::Round($lumaStats.Average, 2)); min: $([Math]::Round($lumaStats.Minimum, 2))"
Write-Host "  Average RGB distance: $([Math]::Round($rgbStats.Average, 2)); min: $([Math]::Round($rgbStats.Minimum, 2))"

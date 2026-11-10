param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$BatchDirectory = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2/generation_batches/base_material_retry_2026_05_28",
    [int]$StartVariant = 209,
    [int]$VariantCount = 24,
    [int]$TileSize = 128,
    [int]$SeamBand = 12,
    [int]$CenterFadeBand = 34,
    [double]$CenterBlend = 0.42
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

function Clamp-Channel([double]$Value) {
    return [Math]::Max(0, [Math]::Min(255, [int][Math]::Round($Value)))
}

function Blend-Color([Drawing.Color]$A, [Drawing.Color]$B, [double]$WeightB) {
    $w = [Math]::Max(0.0, [Math]::Min(1.0, $WeightB))
    $aw = 1.0 - $w
    return [Drawing.Color]::FromArgb(
        255,
        (Clamp-Channel (($A.R * $aw) + ($B.R * $w))),
        (Clamp-Channel (($A.G * $aw) + ($B.G * $w))),
        (Clamp-Channel (($A.B * $aw) + ($B.B * $w)))
    )
}

function Get-BitmapAverageColor([Drawing.Bitmap]$Bitmap, [int]$Step = 8) {
    $sumR = 0.0
    $sumG = 0.0
    $sumB = 0.0
    $count = 0
    for ($y = 0; $y -lt $Bitmap.Height; $y += $Step) {
        for ($x = 0; $x -lt $Bitmap.Width; $x += $Step) {
            $pixel = $Bitmap.GetPixel($x, $y)
            $sumR += $pixel.R
            $sumG += $pixel.G
            $sumB += $pixel.B
            ++$count
        }
    }
    return [pscustomobject]@{ R = $sumR / $count; G = $sumG / $count; B = $sumB / $count }
}

function Limit-Delta([double]$Value, [double]$Limit = 24.0) {
    return [Math]::Max(-$Limit, [Math]::Min($Limit, $Value))
}

function Apply-TileAverageNormalize([Drawing.Bitmap]$Bitmap, $TargetAverage, [double]$Strength = 0.85, [double]$MaxShift = 24.0) {
    $tileAverage = Get-BitmapAverageColor $Bitmap 2
    $deltaR = Limit-Delta (($TargetAverage.R - $tileAverage.R) * $Strength) $MaxShift
    $deltaG = Limit-Delta (($TargetAverage.G - $tileAverage.G) * $Strength) $MaxShift
    $deltaB = Limit-Delta (($TargetAverage.B - $tileAverage.B) * $Strength) $MaxShift
    for ($y = 0; $y -lt $Bitmap.Height; ++$y) {
        for ($x = 0; $x -lt $Bitmap.Width; ++$x) {
            $pixel = $Bitmap.GetPixel($x, $y)
            $Bitmap.SetPixel(
                $x,
                $y,
                [Drawing.Color]::FromArgb(
                    255,
                    (Clamp-Channel ([double]$pixel.R + $deltaR)),
                    (Clamp-Channel ([double]$pixel.G + $deltaG)),
                    (Clamp-Channel ([double]$pixel.B + $deltaB))
                )
            )
        }
    }
}

function New-DownsampledCrop([Drawing.Bitmap]$Source, [int]$X, [int]$Y, [int]$CropSize, [int]$TileSize) {
    $tile = [Drawing.Bitmap]::new($TileSize, $TileSize, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($tile)
    try {
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $sourceRect = [Drawing.Rectangle]::new($X, $Y, $CropSize, $CropSize)
        $destRect = [Drawing.Rectangle]::new(0, 0, $TileSize, $TileSize)
        $graphics.DrawImage($Source, $destRect, $sourceRect, [Drawing.GraphicsUnit]::Pixel)
    }
    finally {
        $graphics.Dispose()
    }
    return $tile
}

function Get-DeterministicNumber([string]$Key, [int]$Modulo) {
    $hash = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [Text.Encoding]::UTF8.GetBytes($Key)
        $digest = $hash.ComputeHash($bytes)
        $value = [BitConverter]::ToUInt32($digest, 0)
        return [int]($value % [uint32]$Modulo)
    }
    finally {
        $hash.Dispose()
    }
}

function SmoothStep([double]$T) {
    $clamped = [Math]::Max(0.0, [Math]::Min(1.0, $T))
    return $clamped * $clamped * (3.0 - (2.0 * $clamped))
}

function Lock-SelfSeamEdges([Drawing.Bitmap]$Bitmap, [int]$Band) {
    $width = $Bitmap.Width
    $height = $Bitmap.Height
    for ($y = 0; $y -lt $height; ++$y) {
        for ($i = 0; $i -lt $Band; ++$i) {
            $leftX = $i
            $rightX = $width - 1 - $i
            $left = $Bitmap.GetPixel($leftX, $y)
            $right = $Bitmap.GetPixel($rightX, $y)
            $average = [Drawing.Color]::FromArgb(
                255,
                (Clamp-Channel (($left.R + $right.R) / 2.0)),
                (Clamp-Channel (($left.G + $right.G) / 2.0)),
                (Clamp-Channel (($left.B + $right.B) / 2.0))
            )
            $weight = [Math]::Pow(($Band - $i) / [double]$Band, 2.0)
            $Bitmap.SetPixel($leftX, $y, (Blend-Color $left $average $weight))
            $Bitmap.SetPixel($rightX, $y, (Blend-Color $right $average $weight))
        }
    }
    for ($x = 0; $x -lt $width; ++$x) {
        for ($i = 0; $i -lt $Band; ++$i) {
            $topY = $i
            $bottomY = $height - 1 - $i
            $top = $Bitmap.GetPixel($x, $topY)
            $bottom = $Bitmap.GetPixel($x, $bottomY)
            $average = [Drawing.Color]::FromArgb(
                255,
                (Clamp-Channel (($top.R + $bottom.R) / 2.0)),
                (Clamp-Channel (($top.G + $bottom.G) / 2.0)),
                (Clamp-Channel (($top.B + $bottom.B) / 2.0))
            )
            $weight = [Math]::Pow(($Band - $i) / [double]$Band, 2.0)
            $Bitmap.SetPixel($x, $topY, (Blend-Color $top $average $weight))
            $Bitmap.SetPixel($x, $bottomY, (Blend-Color $bottom $average $weight))
        }
    }
}

function New-EdgeLockedVariant([Drawing.Bitmap]$CommonTile, [Drawing.Bitmap]$VariationTile, [int]$FadeBand, [double]$CenterBlend) {
    $tile = [Drawing.Bitmap]::new($CommonTile.Width, $CommonTile.Height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    for ($y = 0; $y -lt $CommonTile.Height; ++$y) {
        for ($x = 0; $x -lt $CommonTile.Width; ++$x) {
            $distanceToEdge = [Math]::Min([Math]::Min($x, $CommonTile.Width - 1 - $x), [Math]::Min($y, $CommonTile.Height - 1 - $y))
            $centerWeight = (SmoothStep ($distanceToEdge / [double]$FadeBand)) * $CenterBlend
            $tile.SetPixel($x, $y, (Blend-Color ($CommonTile.GetPixel($x, $y)) ($VariationTile.GetPixel($x, $y)) $centerWeight))
        }
    }
    return $tile
}

function New-TerrainBaseEntry([string]$Family, [string]$Detail, [int]$Variant, [string]$SourcePath, [string]$SourceSheet, [string[]]$Tags, $Gameplay, [int]$SeamBand, [int]$CenterFadeBand, [double]$CenterBlend) {
    $variantText = $Variant.ToString("000")
    return [ordered]@{
        id = "fantasy_neutral.world.base.$Family.$Detail.v$variantText"
        style = "fantasy_neutral"
        scale = "world"
        asset_class = "base"
        owner = $Family
        variant = $Variant
        source_path = $SourcePath
        dimensions = [ordered]@{ width = 128; height = 128 }
        tile_shape = "square"
        edge_contract = "same_family_edge_locked_candidate"
        tags = $Tags
        gameplay = $Gameplay
        status = "candidate"
        generation = [ordered]@{
            method = "ai_material_source_edge_locked_center_variation"
            source_sheet = $SourceSheet
            prompt_summary = "Seventh-pass AI material-sheet extraction with a shared self-seamless family edge and AI-crop variation blended into tile centers to reduce visible lattice while preserving generated-art variation."
            seam_band_px = $SeamBand
            center_fade_band_px = $CenterFadeBand
            center_blend = $CenterBlend
            addresses_validation_issues = @(
                "hard_seam",
                "visible_stamping",
                "faint_grid_artifacts",
                "same_family_edge_contract",
                "water_value_blocking",
                "snow_square_crust_edges"
            )
        }
    }
}

$resolvedSourceRoot = Resolve-TilesetPath $SourceRoot
$resolvedBatchDirectory = Resolve-TilesetPath $BatchDirectory
$tilesPath = Join-Path $resolvedSourceRoot "tiles.json"
$tilesManifest = Read-TilesetJson $tilesPath
$assets = @(Get-TilesetManifestAssets $tilesManifest)
$existingIds = @{}
foreach ($asset in $assets) {
    $existingIds[(Get-TilesetProperty $asset "id" "")] = $true
}

$families = @(
    [pscustomobject]@{
        family = "grass_open"; detail = "soft_meadow_edge_locked_retry"; source = "grass_open_source_ai_2026_05_28.png"
        tags = @("terrain", "open_ground", "passable", "first_vertical_slice", "edge_locked_retry")
        gameplay = [ordered]@{ passability = "passable"; movement_cost = 1; difficult_terrain = $false; blocks_line_of_sight = $false; elevation = 0; cover = "none"; hazard = "none" }
    },
    [pscustomobject]@{
        family = "dirt"; detail = "dry_loam_edge_locked_retry"; source = "dirt_source_ai_2026_05_28.png"
        tags = @("terrain", "dirt", "passable", "first_vertical_slice", "edge_locked_retry")
        gameplay = [ordered]@{ passability = "passable"; movement_cost = 2; difficult_terrain = $true; blocks_line_of_sight = $false; elevation = 0; cover = "none"; hazard = "none" }
    },
    [pscustomobject]@{
        family = "snow"; detail = "wind_crust_edge_locked_retry"; source = "snow_source_ai_2026_05_28.png"
        tags = @("terrain", "snow", "passable", "first_vertical_slice", "edge_locked_retry")
        gameplay = [ordered]@{ passability = "passable"; movement_cost = 2; difficult_terrain = $true; blocks_line_of_sight = $false; elevation = 0; cover = "none"; hazard = "none" }
    },
    [pscustomobject]@{
        family = "water_shallow"; detail = "clear_shoal_edge_locked_retry"; source = "water_shallow_source_ai_2026_05_28.png"
        tags = @("terrain", "water", "shallow_water", "difficult", "first_vertical_slice", "edge_locked_retry")
        gameplay = [ordered]@{ passability = "passable"; movement_cost = 2; difficult_terrain = $true; blocks_line_of_sight = $false; elevation = 0; cover = "none"; hazard = "none" }
    }
)

$added = @()
$skipped = @()
foreach ($familySpec in $families) {
    $sourcePath = Join-Path $resolvedBatchDirectory $familySpec.source
    if (-not (Test-Path -LiteralPath $sourcePath)) {
        throw "Missing source sheet: $sourcePath"
    }

    $sourceBitmap = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $sourcePath).Path)
    try {
        $targetAverage = Get-BitmapAverageColor $sourceBitmap 8
        $commonCropSize = [Math]::Min(640, [Math]::Min($sourceBitmap.Width, $sourceBitmap.Height))
        $commonX = [int](($sourceBitmap.Width - $commonCropSize) / 2)
        $commonY = [int](($sourceBitmap.Height - $commonCropSize) / 2)
        $commonTile = New-DownsampledCrop $sourceBitmap $commonX $commonY $commonCropSize $TileSize
        Apply-TileAverageNormalize $commonTile $targetAverage 0.9 22
        Lock-SelfSeamEdges $commonTile $SeamBand

        $outputDirectory = Join-Path $resolvedSourceRoot "base/$($familySpec.family)"
        New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
        $relativeSourceSheet = ConvertTo-TilesetRelativePath $sourcePath

        for ($i = 0; $i -lt $VariantCount; ++$i) {
            $variant = $StartVariant + $i
            $variantText = $variant.ToString("000")
            $id = "fantasy_neutral.world.base.$($familySpec.family).$($familySpec.detail).v$variantText"
            if ($existingIds.ContainsKey($id)) {
                $skipped += $id
                continue
            }

            $cropSizes = @(224, 256, 288, 320, 352)
            if ($familySpec.family -eq "water_shallow") {
                $cropSizes = @(288, 320, 352, 384)
            }
            $cropSize = [Math]::Min($cropSizes[$i % $cropSizes.Count], [Math]::Min($sourceBitmap.Width, $sourceBitmap.Height))
            $maxX = [Math]::Max(1, $sourceBitmap.Width - $cropSize)
            $maxY = [Math]::Max(1, $sourceBitmap.Height - $cropSize)
            $x = Get-DeterministicNumber "$($familySpec.family)-edge-lock-$variant-x" $maxX
            $y = Get-DeterministicNumber "$($familySpec.family)-edge-lock-$variant-y" $maxY
            $variationTile = New-DownsampledCrop $sourceBitmap $x $y $cropSize $TileSize
            Apply-TileAverageNormalize $variationTile $targetAverage 0.9 24
            $tile = New-EdgeLockedVariant $commonTile $variationTile $CenterFadeBand $CenterBlend
            $variationTile.Dispose()

            $fileName = "fantasy_neutral__world__base__$($familySpec.family)__$($familySpec.detail)__v$variantText.png"
            $outputPath = Join-Path $outputDirectory $fileName
            $tile.Save($outputPath, [Drawing.Imaging.ImageFormat]::Png)
            $tile.Dispose()

            $relativeOutputPath = ConvertTo-TilesetRelativePath $outputPath
            $entry = New-TerrainBaseEntry $familySpec.family $familySpec.detail $variant $relativeOutputPath $relativeSourceSheet $familySpec.tags $familySpec.gameplay $SeamBand $CenterFadeBand $CenterBlend
            $assets += [pscustomobject]$entry
            $existingIds[$id] = $true
            $added += $id
        }

        $commonPath = Join-Path $resolvedBatchDirectory "$($familySpec.family)_edge_locked_common_v$($StartVariant.ToString("000"))_v$(($StartVariant + $VariantCount - 1).ToString("000")).png"
        $commonTile.Save($commonPath, [Drawing.Imaging.ImageFormat]::Png)
        $commonTile.Dispose()
    }
    finally {
        $sourceBitmap.Dispose()
    }
}

$tilesManifest.assets = $assets
$tilesManifest | ConvertTo-Json -Depth 14 | Set-Content -LiteralPath $tilesPath

$report = [pscustomobject]@{
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    source_root = ConvertTo-TilesetRelativePath $resolvedSourceRoot
    batch_directory = ConvertTo-TilesetRelativePath $resolvedBatchDirectory
    start_variant = $StartVariant
    variant_count = $VariantCount
    seam_band_px = $SeamBand
    center_fade_band_px = $CenterFadeBand
    center_blend = $CenterBlend
    added_count = $added.Count
    skipped_existing_count = $skipped.Count
    added_ids = $added
    skipped_existing_ids = $skipped
}

$reportPath = Join-Path $resolvedBatchDirectory "edge_locked_retry_v$($StartVariant.ToString("000"))_v$(($StartVariant + $VariantCount - 1).ToString("000"))_manifest_append_report.json"
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $reportPath

Write-Host "Generated $($added.Count) edge-locked base candidate(s)."
Write-Host "Skipped $($skipped.Count) existing candidate id(s)."
Write-Host "Report: $reportPath"

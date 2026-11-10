param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$BatchDirectory = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2/generation_batches/base_material_retry_2026_05_28",
    [int]$StartVariant = 113,
    [int]$VariantCount = 24,
    [int]$TileSize = 128,
    [int]$EdgeBand = 18,
    [double]$MaxEdgeWeight = 0.74,
    [double]$NormalizeTileAverageStrength = 0.0,
    [double]$MaxAverageShift = 24.0
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

function Clamp-Channel([double]$Value) {
    return [Math]::Max(0, [Math]::Min(255, [int][Math]::Round($Value)))
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
    return [pscustomobject]@{
        R = $sumR / $count
        G = $sumG / $count
        B = $sumB / $count
    }
}

function Get-BitmapEdgeAverage([Drawing.Bitmap]$Bitmap, [string]$Edge, [int]$Band = 2) {
    $sumR = 0.0
    $sumG = 0.0
    $sumB = 0.0
    $count = 0
    for ($y = 0; $y -lt $Bitmap.Height; ++$y) {
        for ($x = 0; $x -lt $Bitmap.Width; ++$x) {
            $include = switch ($Edge) {
                "north" { $y -lt $Band }
                "east" { $x -ge ($Bitmap.Width - $Band) }
                "south" { $y -ge ($Bitmap.Height - $Band) }
                "west" { $x -lt $Band }
            }
            if (-not $include) {
                continue
            }
            $pixel = $Bitmap.GetPixel($x, $y)
            $sumR += $pixel.R
            $sumG += $pixel.G
            $sumB += $pixel.B
            ++$count
        }
    }
    return [pscustomobject]@{
        R = $sumR / $count
        G = $sumG / $count
        B = $sumB / $count
    }
}

function Limit-Delta([double]$Value, [double]$Limit = 30.0) {
    return [Math]::Max(-$Limit, [Math]::Min($Limit, $Value))
}

function Apply-SoftEdgeHarmonize([Drawing.Bitmap]$Bitmap, $TargetAverage, [int]$Band, [double]$MaxWeight) {
    $north = Get-BitmapEdgeAverage $Bitmap "north" 2
    $east = Get-BitmapEdgeAverage $Bitmap "east" 2
    $south = Get-BitmapEdgeAverage $Bitmap "south" 2
    $west = Get-BitmapEdgeAverage $Bitmap "west" 2

    $deltas = @{
        north = [pscustomobject]@{ R = Limit-Delta ($TargetAverage.R - $north.R); G = Limit-Delta ($TargetAverage.G - $north.G); B = Limit-Delta ($TargetAverage.B - $north.B) }
        east = [pscustomobject]@{ R = Limit-Delta ($TargetAverage.R - $east.R); G = Limit-Delta ($TargetAverage.G - $east.G); B = Limit-Delta ($TargetAverage.B - $east.B) }
        south = [pscustomobject]@{ R = Limit-Delta ($TargetAverage.R - $south.R); G = Limit-Delta ($TargetAverage.G - $south.G); B = Limit-Delta ($TargetAverage.B - $south.B) }
        west = [pscustomobject]@{ R = Limit-Delta ($TargetAverage.R - $west.R); G = Limit-Delta ($TargetAverage.G - $west.G); B = Limit-Delta ($TargetAverage.B - $west.B) }
    }

    for ($y = 0; $y -lt $Bitmap.Height; ++$y) {
        for ($x = 0; $x -lt $Bitmap.Width; ++$x) {
            $weights = @()
            if ($y -lt $Band) { $weights += [pscustomobject]@{ Edge = "north"; Weight = $MaxWeight * [Math]::Pow(($Band - $y) / [double]$Band, 2.0) } }
            if ($x -ge ($Bitmap.Width - $Band)) { $weights += [pscustomobject]@{ Edge = "east"; Weight = $MaxWeight * [Math]::Pow(($Band - ($Bitmap.Width - 1 - $x)) / [double]$Band, 2.0) } }
            if ($y -ge ($Bitmap.Height - $Band)) { $weights += [pscustomobject]@{ Edge = "south"; Weight = $MaxWeight * [Math]::Pow(($Band - ($Bitmap.Height - 1 - $y)) / [double]$Band, 2.0) } }
            if ($x -lt $Band) { $weights += [pscustomobject]@{ Edge = "west"; Weight = $MaxWeight * [Math]::Pow(($Band - $x) / [double]$Band, 2.0) } }
            if ($weights.Count -eq 0) {
                continue
            }

            $pixel = $Bitmap.GetPixel($x, $y)
            $r = [double]$pixel.R
            $g = [double]$pixel.G
            $b = [double]$pixel.B
            foreach ($entry in $weights) {
                $delta = $deltas[$entry.Edge]
                $r += $delta.R * $entry.Weight
                $g += $delta.G * $entry.Weight
                $b += $delta.B * $entry.Weight
            }
            $Bitmap.SetPixel($x, $y, [Drawing.Color]::FromArgb($pixel.A, (Clamp-Channel $r), (Clamp-Channel $g), (Clamp-Channel $b)))
        }
    }
}

function Apply-TileAverageNormalize([Drawing.Bitmap]$Bitmap, $TargetAverage, [double]$Strength, [double]$MaxShift) {
    if ($Strength -le 0.0) {
        return
    }

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
                    $pixel.A,
                    (Clamp-Channel ([double]$pixel.R + $deltaR)),
                    (Clamp-Channel ([double]$pixel.G + $deltaG)),
                    (Clamp-Channel ([double]$pixel.B + $deltaB))
                )
            )
        }
    }
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

function New-TerrainBaseEntry([string]$Family, [string]$Detail, [int]$Variant, [string]$SourcePath, [string]$SourceSheet, [string[]]$Tags, $Gameplay) {
    $variantText = $Variant.ToString("000")
    return [ordered]@{
        id = "fantasy_neutral.world.base.$Family.$Detail.v$variantText"
        style = "fantasy_neutral"
        scale = "world"
        asset_class = "base"
        owner = $Family
        variant = $Variant
        source_path = $SourcePath
        dimensions = [ordered]@{
            width = 128
            height = 128
        }
        tile_shape = "square"
        edge_contract = "same_family_soft_edge_candidate"
        tags = $Tags
        gameplay = $Gameplay
        status = "candidate"
        generation = [ordered]@{
            method = "ai_material_source_random_crop_soft_edge_harmonize"
            source_sheet = $SourceSheet
            prompt_summary = "Sixth-pass AI material-sheet extraction using varied crop scales and soft color-only edge harmonizing to reduce lattice artifacts without stamping a shared border texture."
            edge_band_px = $EdgeBand
            max_edge_weight = $MaxEdgeWeight
            normalize_tile_average_strength = $NormalizeTileAverageStrength
            max_average_shift = $MaxAverageShift
            addresses_validation_issues = @(
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

$families = @(
    [pscustomobject]@{
        family = "grass_open"
        detail = "soft_meadow_material_retry"
        source = "grass_open_source_ai_2026_05_28.png"
        tags = @("terrain", "open_ground", "passable", "first_vertical_slice", "material_retry")
        gameplay = [ordered]@{ passability = "passable"; movement_cost = 1; difficult_terrain = $false; blocks_line_of_sight = $false; elevation = 0; cover = "none"; hazard = "none" }
    },
    [pscustomobject]@{
        family = "dirt"
        detail = "dry_loam_material_retry"
        source = "dirt_source_ai_2026_05_28.png"
        tags = @("terrain", "dirt", "passable", "first_vertical_slice", "material_retry")
        gameplay = [ordered]@{ passability = "passable"; movement_cost = 2; difficult_terrain = $true; blocks_line_of_sight = $false; elevation = 0; cover = "none"; hazard = "none" }
    },
    [pscustomobject]@{
        family = "snow"
        detail = "wind_crust_material_retry"
        source = "snow_source_ai_2026_05_28.png"
        tags = @("terrain", "snow", "passable", "first_vertical_slice", "material_retry")
        gameplay = [ordered]@{ passability = "passable"; movement_cost = 2; difficult_terrain = $true; blocks_line_of_sight = $false; elevation = 0; cover = "none"; hazard = "none" }
    },
    [pscustomobject]@{
        family = "water_shallow"
        detail = "clear_shoal_material_retry"
        source = "water_shallow_source_ai_2026_05_28.png"
        tags = @("terrain", "water", "shallow_water", "difficult", "first_vertical_slice", "material_retry")
        gameplay = [ordered]@{ passability = "passable"; movement_cost = 2; difficult_terrain = $true; blocks_line_of_sight = $false; elevation = 0; cover = "none"; hazard = "none" }
    }
)

$added = @()
$skipped = @()
$existingIds = @{}
foreach ($asset in $assets) {
    $existingIds[(Get-TilesetProperty $asset "id" "")] = $true
}

foreach ($familySpec in $families) {
    $sourcePath = Join-Path $resolvedBatchDirectory $familySpec.source
    if (-not (Test-Path -LiteralPath $sourcePath)) {
        throw "Missing source sheet: $sourcePath"
    }

    $sourceBitmap = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $sourcePath).Path)
    try {
        $targetAverage = Get-BitmapAverageColor $sourceBitmap 8
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
            $cropSize = $cropSizes[$i % $cropSizes.Count]
            if ($familySpec.family -eq "water_shallow") {
                $cropSize = @(288, 320, 352, 384)[$i % 4]
            }
            $cropSize = [Math]::Min($cropSize, [Math]::Min($sourceBitmap.Width, $sourceBitmap.Height))
            $maxX = [Math]::Max(1, $sourceBitmap.Width - $cropSize)
            $maxY = [Math]::Max(1, $sourceBitmap.Height - $cropSize)
            $x = Get-DeterministicNumber "$($familySpec.family)-$variant-x" $maxX
            $y = Get-DeterministicNumber "$($familySpec.family)-$variant-y" $maxY

            $tile = [Drawing.Bitmap]::new($TileSize, $TileSize, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
            $graphics = [Drawing.Graphics]::FromImage($tile)
            try {
                $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
                $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                $sourceRect = [Drawing.Rectangle]::new($x, $y, $cropSize, $cropSize)
                $destRect = [Drawing.Rectangle]::new(0, 0, $TileSize, $TileSize)
                $graphics.DrawImage($sourceBitmap, $destRect, $sourceRect, [Drawing.GraphicsUnit]::Pixel)
            }
            finally {
                $graphics.Dispose()
            }

            Apply-TileAverageNormalize $tile $targetAverage $NormalizeTileAverageStrength $MaxAverageShift
            Apply-SoftEdgeHarmonize $tile $targetAverage $EdgeBand $MaxEdgeWeight

            $fileName = "fantasy_neutral__world__base__$($familySpec.family)__$($familySpec.detail)__v$variantText.png"
            $outputPath = Join-Path $outputDirectory $fileName
            $tile.Save($outputPath, [Drawing.Imaging.ImageFormat]::Png)
            $tile.Dispose()

            $relativeOutputPath = ConvertTo-TilesetRelativePath $outputPath
            $entry = New-TerrainBaseEntry $familySpec.family $familySpec.detail $variant $relativeOutputPath $relativeSourceSheet $familySpec.tags $familySpec.gameplay
            $assets += [pscustomobject]$entry
            $existingIds[$id] = $true
            $added += $id
        }
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
    added_count = $added.Count
    skipped_existing_count = $skipped.Count
    added_ids = $added
    skipped_existing_ids = $skipped
}

$reportPath = Join-Path $resolvedBatchDirectory "material_retry_v$($StartVariant.ToString("000"))_v$(($StartVariant + $VariantCount - 1).ToString("000"))_manifest_append_report.json"
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $reportPath

Write-Host "Generated $($added.Count) material retry base candidate(s)."
Write-Host "Skipped $($skipped.Count) existing candidate id(s)."
Write-Host "Report: $reportPath"

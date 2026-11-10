param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$BatchDirectory = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2/generation_batches/base_material_retry_2026_05_28",
    [string[]]$Families = @("grass_open", "snow", "water_shallow", "dirt"),
    [int]$StartVariant = 5,
    [int]$VariantCount = 8,
    [int]$TileSize = 128,
    [int]$CenterFadeBand = 42,
    [double]$CenterBlend = 0.28,
    [switch]$Overwrite
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

function SmoothStep([double]$T) {
    $clamped = [Math]::Max(0.0, [Math]::Min(1.0, $T))
    return $clamped * $clamped * (3.0 - (2.0 * $clamped))
}

function Copy-ResizedBitmap([Drawing.Bitmap]$Source, [int]$Size) {
    if ($Source.Width -eq $Size -and $Source.Height -eq $Size) {
        return [Drawing.Bitmap]::new($Source)
    }

    $bitmap = [Drawing.Bitmap]::new($Size, $Size, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.DrawImage($Source, [Drawing.Rectangle]::new(0, 0, $Size, $Size), 0, 0, $Source.Width, $Source.Height, [Drawing.GraphicsUnit]::Pixel)
    }
    finally {
        $graphics.Dispose()
    }
    return $bitmap
}

function New-SameFamilyAutoTile([Drawing.Bitmap]$CommonTile, [Drawing.Bitmap]$BaseTile, [int]$FadeBand, [double]$Blend) {
    $tile = [Drawing.Bitmap]::new($CommonTile.Width, $CommonTile.Height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    for ($y = 0; $y -lt $CommonTile.Height; ++$y) {
        for ($x = 0; $x -lt $CommonTile.Width; ++$x) {
            $distanceToEdge = [Math]::Min([Math]::Min($x, $CommonTile.Width - 1 - $x), [Math]::Min($y, $CommonTile.Height - 1 - $y))
            $centerWeight = (SmoothStep ($distanceToEdge / [double]$FadeBand)) * $Blend
            $tile.SetPixel($x, $y, (Blend-Color ($CommonTile.GetPixel($x, $y)) ($BaseTile.GetPixel($x, $y)) $centerWeight))
        }
    }
    return $tile
}

function Test-TilesetTag($Asset, [string]$Tag) {
    foreach ($candidate in (ConvertTo-TilesetArray (Get-TilesetProperty $Asset "tags" @()))) {
        if ([string]$candidate -eq $Tag) {
            return $true
        }
    }
    return $false
}

function Get-FamilyDetail([string]$Family) {
    switch ($Family) {
        "grass_open" { return "soft_meadow_edge_locked_auto" }
        "dirt" { return "dry_loam_edge_locked_auto" }
        "snow" { return "wind_crust_edge_locked_auto" }
        "water_shallow" { return "clear_shoal_edge_locked_auto" }
        default { return "edge_locked_auto" }
    }
}

function Get-CommonTilePath([string]$BatchRoot, [string]$Family, [int]$StartVariant, [int]$VariantCount) {
    $endVariant = $StartVariant + $VariantCount - 1
    $expected = Join-Path $BatchRoot "$($Family)_edge_locked_common_v$($StartVariant.ToString("000"))_v$($endVariant.ToString("000")).png"
    if (Test-Path -LiteralPath $expected) {
        return $expected
    }

    $matches = @(Get-ChildItem -LiteralPath $BatchRoot -Filter "$($Family)_edge_locked_common_*.png" | Sort-Object Name)
    if ($matches.Count -eq 0) {
        return ""
    }
    return $matches[-1].FullName
}

function New-SameFamilyAutoEntry([string]$Family, [int]$Mask, [int]$Variant, [string]$SourcePath, [string]$CommonPath, $BaseAsset, [string[]]$Tags, $Gameplay, [int]$CenterFadeBand, [double]$CenterBlend) {
    $maskText = $Mask.ToString("00")
    $variantText = $Variant.ToString("000")
    $baseId = Get-TilesetProperty $BaseAsset "id" ""
    $baseSourcePath = Get-TilesetProperty $BaseAsset "source_path" ""
    return [ordered]@{
        id = "fantasy_neutral.world.same_auto.$Family.mask$maskText.v$variantText"
        style = "fantasy_neutral"
        scale = "world"
        asset_class = "same_auto"
        owner = $Family
        mask = $Mask
        variant = $Variant
        source_path = $SourcePath
        dimensions = [ordered]@{ width = 128; height = 128 }
        tile_shape = "square"
        edge_contract = "same_family_auto_edge_locked_candidate"
        tags = $Tags
        gameplay = $Gameplay
        status = "candidate"
        generation = [ordered]@{
            method = "edge_locked_base_same_auto_derivative"
            source_base_asset_id = $baseId
            source_base_asset_path = $baseSourcePath
            source_common_edge_tile = $CommonPath
            prompt_summary = "Programmatic same-family auto-mask derivative from the active edge_locked_retry base direction. The shared family edge is preserved on every side; only the tile center inherits subdued base variation. Candidate-only, not accepted runtime art."
            center_fade_band_px = $CenterFadeBand
            center_blend = $CenterBlend
            addresses_validation_issues = @(
                "missing_same_auto_candidate_coverage",
                "same_family_edge_contract",
                "visible_stamping",
                "faint_grid_artifacts"
            )
        }
    }
}

$resolvedSourceRoot = Resolve-TilesetPath $SourceRoot
$resolvedBatchDirectory = Resolve-TilesetPath $BatchDirectory
$tilesPath = Join-Path $resolvedSourceRoot "tiles.json"
$tilesManifest = Read-TilesetJson $tilesPath
$assets = @(Get-TilesetManifestAssets $tilesManifest)

$assetById = @{}
for ($i = 0; $i -lt $assets.Count; ++$i) {
    $assetById[(Get-TilesetProperty $assets[$i] "id" "")] = $i
}

$added = @()
$updated = @()
$skipped = @()
$familyReports = @()

foreach ($family in $Families) {
    $activeBases = @($assets | Where-Object {
        (Get-TilesetProperty $_ "owner" "") -eq $family -and
        (Get-TilesetProperty $_ "asset_class" "") -eq "base" -and
        (Get-TilesetProperty $_ "status" "") -in @("candidate", "accepted") -and
        (Test-TilesetTag $_ "edge_locked_retry")
    } | Sort-Object { [int](Get-TilesetProperty $_ "variant" 0) }, { Get-TilesetProperty $_ "id" "" })

    if ($activeBases.Count -eq 0) {
        throw "No active edge_locked_retry base candidates found for family '$family'."
    }

    $commonPath = Get-CommonTilePath $resolvedBatchDirectory $family 209 24
    if ([string]::IsNullOrWhiteSpace($commonPath) -or -not (Test-Path -LiteralPath $commonPath)) {
        throw "Missing edge-locked common tile for family '$family' in $resolvedBatchDirectory."
    }

    $commonSource = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $commonPath).Path)
    $commonTile = $null
    try {
        $commonTile = Copy-ResizedBitmap $commonSource $TileSize
    }
    finally {
        $commonSource.Dispose()
    }

    $outputDirectory = Join-Path $resolvedSourceRoot "same_auto/$family"
    New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
    $tags = @("terrain", "auto_tile", "first_vertical_slice", "edge_locked_retry", "same_family_auto")
    foreach ($tag in (ConvertTo-TilesetArray (Get-TilesetProperty $activeBases[0] "tags" @()))) {
        if ($tag -notin $tags) {
            $tags += [string]$tag
        }
    }
    $gameplay = Get-TilesetProperty $activeBases[0] "gameplay" ([ordered]@{})
    $familyAdded = 0
    $familyUpdated = 0
    $familySkipped = 0

    try {
        for ($mask = 0; $mask -lt 16; ++$mask) {
            for ($i = 0; $i -lt $VariantCount; ++$i) {
                $variant = $StartVariant + $i
                $variantText = $variant.ToString("000")
                $maskText = $mask.ToString("00")
                $id = "fantasy_neutral.world.same_auto.$family.mask$maskText.v$variantText"
                $fileName = "fantasy_neutral__world__same_auto__$family`__mask$maskText`__v$variantText.png"
                $outputPath = Join-Path $outputDirectory $fileName

                if ($assetById.ContainsKey($id) -and -not $Overwrite) {
                    $skipped += $id
                    ++$familySkipped
                    continue
                }

                $baseIndex = (($mask * 7) + ($i * 3)) % $activeBases.Count
                $baseAsset = $activeBases[$baseIndex]
                $basePath = Get-TilesetAssetSourcePath $baseAsset
                if (-not (Test-Path -LiteralPath $basePath)) {
                    throw "Missing source base image for ${id}: $basePath"
                }

                if ($Overwrite -or -not (Test-Path -LiteralPath $outputPath)) {
                    $baseSource = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $basePath).Path)
                    $baseTile = $null
                    $tile = $null
                    try {
                        $baseTile = Copy-ResizedBitmap $baseSource $TileSize
                        $tile = New-SameFamilyAutoTile $commonTile $baseTile $CenterFadeBand $CenterBlend
                        $tile.Save($outputPath, [Drawing.Imaging.ImageFormat]::Png)
                    }
                    finally {
                        if ($null -ne $tile) { $tile.Dispose() }
                        if ($null -ne $baseTile) { $baseTile.Dispose() }
                        $baseSource.Dispose()
                    }
                }

                $relativeOutputPath = ConvertTo-TilesetRelativePath $outputPath
                $relativeCommonPath = ConvertTo-TilesetRelativePath $commonPath
                $entry = [pscustomobject](New-SameFamilyAutoEntry $family $mask $variant $relativeOutputPath $relativeCommonPath $baseAsset $tags $gameplay $CenterFadeBand $CenterBlend)
                if ($assetById.ContainsKey($id)) {
                    $assets[$assetById[$id]] = $entry
                    $updated += $id
                    ++$familyUpdated
                }
                else {
                    $assets += $entry
                    $assetById[$id] = $assets.Count - 1
                    $added += $id
                    ++$familyAdded
                }
            }
        }
    }
    finally {
        $commonTile.Dispose()
    }

    $familyReports += [pscustomobject]@{
        family = $family
        active_base_count = $activeBases.Count
        common_edge_tile = ConvertTo-TilesetRelativePath $commonPath
        added_count = $familyAdded
        updated_count = $familyUpdated
        skipped_existing_count = $familySkipped
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
    tile_size = $TileSize
    center_fade_band_px = $CenterFadeBand
    center_blend = $CenterBlend
    added_count = $added.Count
    updated_count = $updated.Count
    skipped_existing_count = $skipped.Count
    families = $familyReports
    added_ids = $added
    updated_ids = $updated
    skipped_existing_ids = $skipped
}

$reportPath = Join-Path $resolvedBatchDirectory "same_family_auto_from_edge_locked_v$($StartVariant.ToString("000"))_v$(($StartVariant + $VariantCount - 1).ToString("000"))_manifest_append_report.json"
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $reportPath

Write-Host "Generated $($added.Count) same-family auto candidate(s)."
Write-Host "Updated $($updated.Count) existing candidate id(s)."
Write-Host "Skipped $($skipped.Count) existing candidate id(s)."
Write-Host "Report: $reportPath"

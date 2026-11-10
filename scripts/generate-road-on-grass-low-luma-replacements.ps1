param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$OutputDirectory = "",
    [string]$WorkOrderId = "fn_v2_way_dirt_road_on_grass_open",
    [string]$WayOwner = "way_dirt_road",
    [string]$GroundOwner = "grass_open",
    [int]$TileSize = 128
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")
Add-Type -AssemblyName System.Drawing

$resolvedSourceRoot = Resolve-TilesetPath $SourceRoot
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $resolvedSourceRoot "review/contextual_candidates"
}
$resolvedOutputDirectory = Resolve-TilesetPath $OutputDirectory
New-Item -ItemType Directory -Force -Path $resolvedOutputDirectory | Out-Null

$wayDirectory = Join-Path $resolvedSourceRoot "way_segment/way_dirt_road_on_grass_open"
New-Item -ItemType Directory -Force -Path $wayDirectory | Out-Null

function Write-CleanJsonFile([string]$Path, $Value) {
    $json = $Value | ConvertTo-Json -Depth 18
    if ($json -is [array]) {
        $json = $json -join "`n"
    }
    $json = $json -replace "`r`n", "`n"
    $json = $json -replace "`r", "`n"
    $json = (($json -split "`n") | ForEach-Object { $_.TrimEnd() }) -join "`n"
    [System.IO.File]::WriteAllText($Path, $json.TrimEnd() + "`n", [System.Text.UTF8Encoding]::new($false))
}

function Format-MaskToken([int]$Mask) {
    return "mask$($Mask.ToString("000"))"
}

function Invoke-WithBitmap([string]$Path, [scriptblock]$Body) {
    $bitmap = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $Path).Path)
    try {
        & $Body $bitmap
    }
    finally {
        $bitmap.Dispose()
    }
}

function New-ContrastTunedRoadPng([string]$SourcePath, [string]$OutputPath) {
    Invoke-WithBitmap $SourcePath {
        param($source)
        $bitmap = [Drawing.Bitmap]::new($source.Width, $source.Height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            for ($y = 0; $y -lt $source.Height; ++$y) {
                for ($x = 0; $x -lt $source.Width; ++$x) {
                    $pixel = $source.GetPixel($x, $y)
                    if ($pixel.A -eq 0) {
                        $bitmap.SetPixel($x, $y, [Drawing.Color]::Transparent)
                        continue
                    }

                    $alphaBoost = if ($pixel.A -lt 160) { 22 } else { 8 }
                    $newAlpha = [Math]::Min(255, $pixel.A + $alphaBoost)
                    $roadCore = $pixel.A -ge 48
                    $rBoost = if ($roadCore) { 24 } else { 12 }
                    $gBoost = if ($roadCore) { 16 } else { 9 }
                    $bBoost = if ($roadCore) { 8 } else { 4 }
                    $r = [Math]::Min(255, $pixel.R + $rBoost)
                    $g = [Math]::Min(255, $pixel.G + $gBoost)
                    $b = [Math]::Min(255, $pixel.B + $bBoost)
                    $bitmap.SetPixel($x, $y, [Drawing.Color]::FromArgb($newAlpha, $r, $g, $b))
                }
            }
            $bitmap.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
        }
        finally {
            $bitmap.Dispose()
        }
    }
}

function Get-AnchorWindow([string]$Direction, [int]$Size) {
    $center = [int]($Size / 2)
    $half = 22
    $depth = 9
    $corner = 20
    switch ($Direction) {
        "N" { return [Drawing.Rectangle]::new($center - $half, 0, ($half * 2) + 1, $depth) }
        "E" { return [Drawing.Rectangle]::new($Size - $depth, $center - $half, $depth, ($half * 2) + 1) }
        "S" { return [Drawing.Rectangle]::new($center - $half, $Size - $depth, ($half * 2) + 1, $depth) }
        "W" { return [Drawing.Rectangle]::new(0, $center - $half, $depth, ($half * 2) + 1) }
        "NE" { return [Drawing.Rectangle]::new($Size - $corner, 0, $corner, $corner) }
        "SE" { return [Drawing.Rectangle]::new($Size - $corner, $Size - $corner, $corner, $corner) }
        "SW" { return [Drawing.Rectangle]::new(0, $Size - $corner, $corner, $corner) }
        "NW" { return [Drawing.Rectangle]::new(0, 0, $corner, $corner) }
    }
    throw "Unknown direction '$Direction'."
}

function Get-MaskDirections([int]$Mask) {
    $bits = @(
        @{ Name = "N"; Bit = 1 },
        @{ Name = "NE"; Bit = 2 },
        @{ Name = "E"; Bit = 4 },
        @{ Name = "SE"; Bit = 8 },
        @{ Name = "S"; Bit = 16 },
        @{ Name = "SW"; Bit = 32 },
        @{ Name = "W"; Bit = 64 },
        @{ Name = "NW"; Bit = 128 }
    )
    return @($bits | Where-Object { ($Mask -band $_.Bit) -ne 0 } | ForEach-Object { $_.Name })
}

function Get-AnchorCoverage([Drawing.Bitmap]$Bitmap, [string]$Direction) {
    $window = Get-AnchorWindow $Direction $Bitmap.Width
    $total = 0
    $covered = 0
    for ($y = $window.Y; $y -lt ($window.Y + $window.Height); ++$y) {
        for ($x = $window.X; $x -lt ($window.X + $window.Width); ++$x) {
            ++$total
            if ($Bitmap.GetPixel($x, $y).A -ge 24) {
                ++$covered
            }
        }
    }
    if ($total -gt 0) {
        return $covered / $total
    }
    return 0.0
}

function Test-GeneratedAnchorContract($GeneratedAssets) {
    $directions = @("N", "NE", "E", "SE", "S", "SW", "W", "NW")
    $errors = @()
    $anchors = @()
    foreach ($asset in $GeneratedAssets) {
        $bitmap = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $asset.path).Path)
        try {
            $present = @(Get-MaskDirections $asset.mask)
            foreach ($direction in $directions) {
                $coverage = Get-AnchorCoverage $bitmap $direction
                $expected = $direction -in $present
                $status = "ok"
                if ($expected -and $coverage -lt 0.22) {
                    $status = "fail"
                    $errors += [pscustomobject]@{
                        issue = "low_luma_replacement_anchor_missing"
                        mask = Format-MaskToken $asset.mask
                        variant = $asset.variant
                        direction = $direction
                        coverage = [Math]::Round($coverage, 3)
                    }
                }
                if ((-not $expected) -and $coverage -gt 0.18) {
                    $status = "fail"
                    $errors += [pscustomobject]@{
                        issue = "low_luma_replacement_anchor_leak"
                        mask = Format-MaskToken $asset.mask
                        variant = $asset.variant
                        direction = $direction
                        coverage = [Math]::Round($coverage, 3)
                    }
                }
                $anchors += [pscustomobject]@{
                    mask = Format-MaskToken $asset.mask
                    variant = $asset.variant
                    direction = $direction
                    expected_exit = $expected
                    coverage = [Math]::Round($coverage, 3)
                    status = $status
                }
            }
        }
        finally {
            $bitmap.Dispose()
        }
    }
    return [pscustomobject]@{
        summary = [pscustomobject]@{
            generated_assets = $GeneratedAssets.Count
            checked_anchors = $anchors.Count
            error_count = $errors.Count
        }
        errors = $errors
        anchors = $anchors
    }
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

$waysPath = Join-Path $resolvedSourceRoot "ways.json"
$ways = Get-Content -LiteralPath $waysPath -Raw | ConvertFrom-Json
$wayAssets = @(ConvertTo-TilesetArray (Get-TilesetProperty $ways "assets" @()))
$existingIds = @{}
foreach ($asset in $wayAssets) {
    $existingIds[(Get-TilesetProperty $asset "id" "")] = $true
}

$targetMasks = @(1, 4, 5, 16, 20, 64, 65, 80, 85)
$replacements = @()
foreach ($mask in $targetMasks) {
    $replacements += [pscustomobject]@{ mask = $mask; source_variant = 2; target_variant = 20 }
    $replacements += [pscustomobject]@{ mask = $mask; source_variant = 4; target_variant = 18 }
    $replacements += [pscustomobject]@{ mask = $mask; source_variant = 5; target_variant = 19 }
}

$generatedAssets = @()
foreach ($replacement in $replacements) {
    $maskToken = Format-MaskToken $replacement.mask
    $sourceVariantToken = "v$($replacement.source_variant.ToString("000"))"
    $targetVariantToken = "v$($replacement.target_variant.ToString("000"))"
    $sourceId = "fantasy_neutral.world.way_segment.way_dirt_road_on_grass_open.$maskToken.$sourceVariantToken"
    $sourceAsset = $wayAssets | Where-Object { (Get-TilesetProperty $_ "id" "") -eq $sourceId } | Select-Object -First 1
    if ($null -eq $sourceAsset) {
        throw "Could not find source asset '$sourceId'."
    }
    $sourcePath = Get-TilesetAssetSourcePath $sourceAsset
    $fileName = "fantasy_neutral__world__way_segment__way_dirt_road_on_grass_open__$maskToken`__$targetVariantToken.png"
    $outputPath = Join-Path $wayDirectory $fileName
    New-ContrastTunedRoadPng $sourcePath $outputPath
    $generatedAssets += [pscustomobject]@{
        mask = [int]$replacement.mask
        mask_token = $maskToken
        source_variant = [int]$replacement.source_variant
        variant = [int]$replacement.target_variant
        variant_token = $targetVariantToken
        source_id = $sourceId
        id = "fantasy_neutral.world.way_segment.way_dirt_road_on_grass_open.$maskToken.$targetVariantToken"
        path = $outputPath
        source_path = ConvertTo-TilesetRelativePath $outputPath
    }
}

$anchorReport = Test-GeneratedAnchorContract $generatedAssets
$anchorReportPath = Resolve-TilesetPath "build/road_grass_low_luma_replacements_v018_v020_anchor_report.json"
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $anchorReportPath) | Out-Null
Write-CleanJsonFile $anchorReportPath ([pscustomobject]@{
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    work_order_id = $WorkOrderId
    masks = @($targetMasks | ForEach-Object { Format-MaskToken $_ })
    variants = @(18, 19, 20)
    anchor_contract = "alpha-window 8-neighbor contract preserved from v004/v005 source geometry"
    thresholds = [pscustomobject]@{
        present_min_coverage = 0.22
        absent_max_coverage = 0.18
    }
    summary = $anchorReport.summary
    errors = $anchorReport.errors
    anchors = $anchorReport.anchors
})
if ($anchorReport.summary.error_count -gt 0) {
    throw "Low-luma replacement anchor validation failed with $($anchorReport.summary.error_count) issue(s). See $anchorReportPath."
}

foreach ($asset in $wayAssets) {
    if ((Get-TilesetProperty $asset "work_order_id" "") -eq $WorkOrderId -and
        [int](Get-TilesetProperty $asset "mask" -1) -in $targetMasks -and
        [int](Get-TilesetProperty $asset "variant" 0) -in @(2, 4, 5) -and
        (Get-TilesetProperty $asset "status" "") -eq "candidate") {
        $asset.status = "needs_repaint"
        $tags = [System.Collections.Generic.List[string]]::new()
        foreach ($tag in (ConvertTo-TilesetArray (Get-TilesetProperty $asset "tags" @()))) {
            if (-not $tags.Contains([string]$tag)) { [void]$tags.Add([string]$tag) }
        }
        foreach ($tag in @("low_luma_at_app_scale", "replaced_by_v018_v020_contrast_tune")) {
            if (-not $tags.Contains($tag)) { [void]$tags.Add($tag) }
        }
        $asset.tags = @($tags)
        $asset.validation = [pscustomobject]@{
            human_review = "low_luma_app_scale_replaced_by_v018_v020_candidate_tune"
            notes = "Demoted from active candidate coverage after app-scale contrast review found this older original-mask variant below the suggested luminance-delta floor. Retained as repaint evidence; v018-v020 provide active tuned replacements."
        }
    }
}

$newManifestAssets = @()
foreach ($generated in $generatedAssets) {
    $manifestAsset = [pscustomobject]@{
        id = $generated.id
        style = "fantasy_neutral"
        scale = "world"
        asset_class = "way_segment"
        owner = $WayOwner
        work_order_id = $WorkOrderId
        mask = $generated.mask
        variant = $generated.variant
        source_path = $generated.source_path
        dimensions = [pscustomobject]@{
            width = $TileSize
            height = $TileSize
        }
        tile_shape = "square"
        transparent = $true
        edge_contract = "integrated_ground_low_luma_replacement_anchor_contract_v018_v020"
        tags = @(
            "contextual_mask_fit_candidate",
            "integrated_way_ground",
            $GroundOwner,
            "road_overlay_shortcut",
            "mask_anchor_aligned",
            "app_scale_contrast_tuned_candidate",
            "replaces_low_luma_v004_v005",
            "v018_v020_low_luma_replacement"
        )
        context = [pscustomobject]@{
            way_family = $WayOwner
            ground = $GroundOwner
            surface_interaction = "roadbed cuts into grass with softened shoulders and tuned app-scale contrast"
            mask_mapping_status = "low_luma_replacement_v$($generated.variant.ToString("000"))_from_v$($generated.source_variant.ToString("000"))_anchor_geometry_preserved"
        }
        status = "candidate"
        validation = [pscustomobject]@{
            human_review = "scripted_low_luma_replacement_anchor_check_passed_candidate_only"
            notes = "Generated by contrast-tuning the v004/v005 source geometry after app-scale review identified low luminance deltas. Anchor checks passed, but assets remain candidate-only until app-scale review is refreshed."
        }
    }
    if ($existingIds.ContainsKey($generated.id)) {
        $ways.assets = @($ways.assets | ForEach-Object {
            if ((Get-TilesetProperty $_ "id" "") -eq $generated.id) { $manifestAsset } else { $_ }
        })
    }
    else {
        $newManifestAssets += $manifestAsset
    }
}
if ($newManifestAssets.Count -gt 0) {
    $ways.assets = @($ways.assets) + $newManifestAssets
}
Write-CleanJsonFile $waysPath $ways

$bundle = Get-TilesetV2ManifestBundle $resolvedSourceRoot
$tileAssets = @(Get-TilesetManifestAssets $bundle.Tiles)
$grassAssets = @($tileAssets | Where-Object {
    (Get-TilesetProperty $_ "owner" "") -eq $GroundOwner -and
    (Get-TilesetProperty $_ "asset_class" "") -eq "base" -and
    (Get-TilesetProperty $_ "status" "") -in @("candidate", "accepted")
})
$contactTileSize = 56
$labelWidth = 96
$rowHeight = $contactTileSize + 22
$contactPath = Join-Path $resolvedOutputDirectory "way_dirt_road_on_grass_open_low_luma_replacements_v018_v020_contact_sheet.png"
$contact = New-TilesetReviewBitmap ($labelWidth + (6 * $contactTileSize) + 22) (30 + ($targetMasks.Count * $rowHeight) + 10)
try {
    Draw-Label $contact.Graphics "low-luma replacements: old v002/v004/v005 vs tuned v020/v018/v019" 8 6 520
    for ($row = 0; $row -lt $targetMasks.Count; ++$row) {
        $mask = [int]$targetMasks[$row]
        $y = 30 + ($row * $rowHeight)
        Draw-Label $contact.Graphics (Format-MaskToken $mask) 8 ($y + 18) 78
        $columns = @(
            [pscustomobject]@{ label = "old v002"; variant = 2; generated = $false },
            [pscustomobject]@{ label = "new v020"; variant = 20; generated = $true },
            [pscustomobject]@{ label = "old v004"; variant = 4; generated = $false },
            [pscustomobject]@{ label = "new v018"; variant = 18; generated = $true },
            [pscustomobject]@{ label = "old v005"; variant = 5; generated = $false },
            [pscustomobject]@{ label = "new v019"; variant = 19; generated = $true }
        )
        for ($col = 0; $col -lt $columns.Count; ++$col) {
            $column = $columns[$col]
            $x = $labelWidth + ($col * $contactTileSize)
            $dest = [Drawing.Rectangle]::new($x, $y, $contactTileSize, $contactTileSize)
            $grass = if ($grassAssets.Count -gt 0) { $grassAssets[(Get-TilesetStableIndex $col $row 1187 $grassAssets.Count)] } else { $null }
            Draw-TilesetAsset $contact.Graphics $grass $dest
            if ($column.generated) {
                $asset = $generatedAssets | Where-Object { $_.mask -eq $mask -and $_.variant -eq $column.variant } | Select-Object -First 1
                $bitmapPath = $asset.path
            }
            else {
                $sourceId = "fantasy_neutral.world.way_segment.way_dirt_road_on_grass_open.$(Format-MaskToken $mask).v$($column.variant.ToString("000"))"
                $sourceAsset = $wayAssets | Where-Object { (Get-TilesetProperty $_ "id" "") -eq $sourceId } | Select-Object -First 1
                $bitmapPath = Get-TilesetAssetSourcePath $sourceAsset
            }
            Invoke-WithBitmap $bitmapPath {
                param($bitmap)
                $contact.Graphics.DrawImage($bitmap, $dest, 0, 0, $bitmap.Width, $bitmap.Height, [Drawing.GraphicsUnit]::Pixel)
            }
            Draw-Label $contact.Graphics $column.label $x ($y + $contactTileSize) $contactTileSize
        }
    }
    $contact.Bitmap.Save($contactPath, [Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $contact.Graphics.Dispose()
    $contact.Bitmap.Dispose()
}

$reportPath = Join-Path $resolvedOutputDirectory "way_dirt_road_on_grass_open_low_luma_replacements_v018_v020_review.json"
Write-CleanJsonFile $reportPath ([pscustomobject]@{
    schema_version = 1
    generated_at_local = (Get-Date).ToString("yyyy-MM-ddTHH:mm:ssK")
    work_order_id = $WorkOrderId
    status = "candidate_only_low_luma_replacements"
    masks = @($targetMasks | ForEach-Object { Format-MaskToken $_ })
    source_variants_demoted = @(2, 4, 5)
    replacement_variants = @(18, 19, 20)
    generated_asset_count = $generatedAssets.Count
    generated_assets = @($generatedAssets | ForEach-Object {
        [pscustomobject]@{
            id = $_.id
            source_id = $_.source_id
            source_path = $_.source_path
            mask = $_.mask_token
            variant = $_.variant
        }
    })
    evidence = [pscustomobject]@{
        contact_sheet = ConvertTo-TilesetRelativePath $contactPath
        anchor_report = ConvertTo-TilesetRelativePath $anchorReportPath
    }
    open_acceptance_holds = @(
        "candidate_only",
        "app_scale_contrast_review_must_be_refreshed",
        "mask255_repaint_needs_app_scale_review"
    )
})

Write-Host "Generated road-on-grass low-luma replacements:"
Write-Host "  Assets:        $($generatedAssets.Count)"
Write-Host "  Contact sheet: $contactPath"
Write-Host "  Review:        $reportPath"
Write-Host "  Anchor report: $anchorReportPath"

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

function Get-AnchorPoint([string]$Direction, [int]$Size, [int]$Outset) {
    $center = $Size / 2.0
    switch ($Direction) {
        "N" { return [Drawing.PointF]::new($center, -$Outset) }
        "NE" { return [Drawing.PointF]::new($Size + $Outset, -$Outset) }
        "E" { return [Drawing.PointF]::new($Size + $Outset, $center) }
        "SE" { return [Drawing.PointF]::new($Size + $Outset, $Size + $Outset) }
        "S" { return [Drawing.PointF]::new($center, $Size + $Outset) }
        "SW" { return [Drawing.PointF]::new(-$Outset, $Size + $Outset) }
        "W" { return [Drawing.PointF]::new(-$Outset, $center) }
        "NW" { return [Drawing.PointF]::new(-$Outset, -$Outset) }
    }
    throw "Unknown direction '$Direction'."
}

function New-SeededRandom([int]$Mask, [int]$Variant) {
    return [Random]::new((($Mask * 1103515245) -bxor ($Variant * 2654435761)) -band 0x7fffffff)
}

function Get-RandomJitter([Random]$Random, [double]$Magnitude) {
    return ([double]$Random.NextDouble() * ($Magnitude * 2.0)) - $Magnitude
}

function Add-RoadCurve(
    [Drawing.Drawing2D.GraphicsPath]$Path,
    [Drawing.PointF]$Start,
    [Drawing.PointF]$End,
    [Random]$Random,
    [double]$CurveJitter
) {
    $dx = $End.X - $Start.X
    $dy = $End.Y - $Start.Y
    $length = [Math]::Max(1.0, [Math]::Sqrt(($dx * $dx) + ($dy * $dy)))
    $nx = -$dy / $length
    $ny = $dx / $length
    $jitter1 = Get-RandomJitter $Random $CurveJitter
    $jitter2 = Get-RandomJitter $Random $CurveJitter
    $c1 = [Drawing.PointF]::new(
        [float]($Start.X + ($dx * 0.34) + ($nx * $jitter1)),
        [float]($Start.Y + ($dy * 0.34) + ($ny * $jitter1))
    )
    $c2 = [Drawing.PointF]::new(
        [float]($Start.X + ($dx * 0.76) + ($nx * $jitter2)),
        [float]($Start.Y + ($dy * 0.76) + ($ny * $jitter2))
    )
    $Path.StartFigure()
    $Path.AddBezier($Start, $c1, $c2, $End)
}

function Add-RoadSegmentForDirection(
    [Drawing.Drawing2D.GraphicsPath]$Path,
    [string]$Direction,
    [int]$Size,
    [Random]$Random
) {
    $center = [Drawing.PointF]::new($Size / 2.0, $Size / 2.0)
    $end = Get-AnchorPoint $Direction $Size 10
    Add-RoadCurve $Path $center $end $Random 4.0
}

function Add-OppositeRoadSegment(
    [Drawing.Drawing2D.GraphicsPath]$Path,
    [string]$A,
    [string]$B,
    [int]$Size,
    [Random]$Random
) {
    $start = Get-AnchorPoint $A $Size 10
    $end = Get-AnchorPoint $B $Size 10
    Add-RoadCurve $Path $start $end $Random 2.0
}

function New-RoadPath([int]$Mask, [int]$Variant, [int]$Size) {
    $random = New-SeededRandom $Mask $Variant
    $directions = [System.Collections.Generic.List[string]]::new()
    foreach ($direction in (Get-MaskDirections $Mask)) {
        [void]$directions.Add($direction)
    }

    $path = [Drawing.Drawing2D.GraphicsPath]::new()
    $opposites = @(
        @("N", "S"),
        @("E", "W"),
        @("NE", "SW"),
        @("NW", "SE")
    )
    foreach ($pair in $opposites) {
        if ($directions.Contains($pair[0]) -and $directions.Contains($pair[1])) {
            Add-OppositeRoadSegment $path $pair[0] $pair[1] $Size $random
            [void]$directions.Remove($pair[0])
            [void]$directions.Remove($pair[1])
        }
    }

    foreach ($direction in $directions) {
        Add-RoadSegmentForDirection $path $direction $Size $random
    }
    return $path
}

function Invoke-WithPen([Drawing.Color]$Color, [float]$Width, [scriptblock]$Body) {
    $pen = [Drawing.Pen]::new($Color, $Width)
    $pen.StartCap = [Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [Drawing.Drawing2D.LineCap]::Round
    $pen.LineJoin = [Drawing.Drawing2D.LineJoin]::Round
    try {
        & $Body $pen
    }
    finally {
        $pen.Dispose()
    }
}

function Apply-RoadTexture([Drawing.Bitmap]$Bitmap, [int]$Mask, [int]$Variant) {
    $random = New-SeededRandom ($Mask + 17) ($Variant + 29)
    for ($y = 0; $y -lt $Bitmap.Height; ++$y) {
        for ($x = 0; $x -lt $Bitmap.Width; ++$x) {
            $pixel = $Bitmap.GetPixel($x, $y)
            if ($pixel.A -lt 18) {
                continue
            }
            $noise = $random.Next(-7, 8)
            $grain = if ((($x * 3 + $y * 5 + $Variant * 11 + $Mask) % 23) -eq 0) { $random.Next(-18, -5) } else { 0 }
            $r = [Math]::Min(255, [Math]::Max(0, $pixel.R + $noise + $grain))
            $g = [Math]::Min(255, [Math]::Max(0, $pixel.G + [Math]::Floor($noise * 0.7) + $grain))
            $b = [Math]::Min(255, [Math]::Max(0, $pixel.B + [Math]::Floor($noise * 0.45) + [Math]::Floor($grain * 0.55)))
            $Bitmap.SetPixel($x, $y, [Drawing.Color]::FromArgb($pixel.A, [int]$r, [int]$g, [int]$b))
        }
    }
}

function New-CompositeRoadPng([int]$Mask, [int]$Variant, [string]$OutputPath, [int]$Size) {
    $bitmap = [Drawing.Bitmap]::new($Size, $Size, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
    $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.Clear([Drawing.Color]::Transparent)
    $roadPath = New-RoadPath $Mask $Variant $Size

    $toneShift = ($Variant - 11) * 4
    try {
        Invoke-WithPen ([Drawing.Color]::FromArgb(116, 97, 73, 43)) 48.0 {
            param($pen)
            $graphics.DrawPath($pen, $roadPath)
        }
        Invoke-WithPen ([Drawing.Color]::FromArgb(204, 146 + $toneShift, 105 + [Math]::Floor($toneShift * 0.6), 62)) 40.0 {
            param($pen)
            $graphics.DrawPath($pen, $roadPath)
        }
        Invoke-WithPen ([Drawing.Color]::FromArgb(246, 177 + $toneShift, 128 + [Math]::Floor($toneShift * 0.55), 74)) 31.0 {
            param($pen)
            $graphics.DrawPath($pen, $roadPath)
        }
        Invoke-WithPen ([Drawing.Color]::FromArgb(92, 116, 82, 47)) 2.0 {
            param($pen)
            $pen.DashStyle = [Drawing.Drawing2D.DashStyle]::Dot
            $graphics.DrawPath($pen, $roadPath)
        }
    }
    finally {
        $roadPath.Dispose()
        $graphics.Dispose()
    }

    Apply-RoadTexture $bitmap $Mask $Variant
    try {
        $bitmap.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $bitmap.Dispose()
    }
}

function Test-AlphaPixel([System.Drawing.Color]$Pixel) {
    return $Pixel.A -ge 24
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

function Get-AnchorCoverage([Drawing.Bitmap]$Bitmap, [string]$Direction) {
    $window = Get-AnchorWindow $Direction $Bitmap.Width
    $total = 0
    $covered = 0
    for ($y = $window.Y; $y -lt ($window.Y + $window.Height); ++$y) {
        for ($x = $window.X; $x -lt ($window.X + $window.Width); ++$x) {
            ++$total
            if (Test-AlphaPixel $Bitmap.GetPixel($x, $y)) {
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
                        issue = "composite_way_mask_anchor_missing"
                        mask = Format-MaskToken $asset.mask
                        variant = $asset.variant
                        direction = $direction
                        coverage = [Math]::Round($coverage, 3)
                    }
                }
                if ((-not $expected) -and $coverage -gt 0.18) {
                    $status = "fail"
                    $errors += [pscustomobject]@{
                        issue = "composite_way_mask_anchor_leak"
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

$masks = @(25, 76, 145, 196)
$variants = @(10, 11, 12, 13)
$generatedAssets = @()
foreach ($mask in $masks) {
    foreach ($variant in $variants) {
        $maskToken = Format-MaskToken $mask
        $variantToken = "v$($variant.ToString("000"))"
        $fileName = "fantasy_neutral__world__way_segment__way_dirt_road_on_grass_open__$maskToken`__$variantToken.png"
        $path = Join-Path $wayDirectory $fileName
        New-CompositeRoadPng $mask $variant $path $TileSize
        $generatedAssets += [pscustomobject]@{
            mask = $mask
            mask_token = $maskToken
            variant = $variant
            variant_token = $variantToken
            id = "fantasy_neutral.world.way_segment.way_dirt_road_on_grass_open.$maskToken.$variantToken"
            path = $path
            source_path = ConvertTo-TilesetRelativePath $path
            directions = @(Get-MaskDirections $mask)
        }
    }
}

$anchorReport = Test-GeneratedAnchorContract $generatedAssets
$anchorReportPath = Resolve-TilesetPath "build/road_grass_composite_masks_v010_v013_anchor_report.json"
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $anchorReportPath) | Out-Null
Write-CleanJsonFile $anchorReportPath ([pscustomobject]@{
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    work_order_id = $WorkOrderId
    masks = @($masks | ForEach-Object { Format-MaskToken $_ })
    variants = $variants
    anchor_contract = "8-neighbor alpha-window edge and corner contract for composite route masks"
    thresholds = [pscustomobject]@{
        present_min_coverage = 0.22
        absent_max_coverage = 0.18
    }
    summary = $anchorReport.summary
    errors = $anchorReport.errors
    anchors = $anchorReport.anchors
})
if ($anchorReport.summary.error_count -gt 0) {
    throw "Composite road mask anchor validation failed with $($anchorReport.summary.error_count) issue(s). See $anchorReportPath."
}

$waysPath = Join-Path $resolvedSourceRoot "ways.json"
$ways = Get-Content -LiteralPath $waysPath -Raw | ConvertFrom-Json
$existingIds = @{}
foreach ($asset in (ConvertTo-TilesetArray (Get-TilesetProperty $ways "assets" @()))) {
    $existingIds[(Get-TilesetProperty $asset "id" "")] = $true
}

$newManifestAssets = @()
foreach ($generated in $generatedAssets) {
    $directionLabel = ($generated.directions -join "_")
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
        edge_contract = "integrated_ground_composite_route_mask_anchor_contract_v010_v013"
        tags = @(
            "contextual_mask_fit_candidate",
            "integrated_way_ground",
            $GroundOwner,
            "road_overlay_shortcut",
            "mask_anchor_aligned",
            "route_continuity_extension_candidate",
            "composite_route_mask_candidate",
            "v010_v013_composite_route_candidate"
        )
        context = [pscustomobject]@{
            way_family = $WayOwner
            ground = $GroundOwner
            surface_interaction = "roadbed cuts into grass with softened shoulders and dirt bleed"
            mask_mapping_status = "composite_route_mask_v$($generated.variant.ToString("000"))_anchor_aligned_$directionLabel"
        }
        status = "candidate"
        validation = [pscustomobject]@{
            human_review = "scripted_composite_route_anchor_check_passed_candidate_only"
            notes = "Generated to cover composite route masks found by full route-layout review. Anchor checks passed for the declared 8-neighbor exits, but assets remain candidate-only; app-scale contrast and mask255 starburst holds are still open."
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
$contactTileSize = 64
$labelWidth = 96
$rowHeight = $contactTileSize + 22
$contactPath = Join-Path $resolvedOutputDirectory "way_dirt_road_on_grass_open_composite_masks_v010_v013_contact_sheet.png"
$contact = New-TilesetReviewBitmap ($labelWidth + ($variants.Count * $contactTileSize) + 20) (30 + ($masks.Count * $rowHeight) + 10)
try {
    Draw-Label $contact.Graphics "road-on-grass composite mask candidates, v010-v013" 8 6 330
    for ($row = 0; $row -lt $masks.Count; ++$row) {
        $mask = [int]$masks[$row]
        $y = 30 + ($row * $rowHeight)
        Draw-Label $contact.Graphics (Format-MaskToken $mask) 8 ($y + 20) 78
        for ($col = 0; $col -lt $variants.Count; ++$col) {
            $variant = [int]$variants[$col]
            $x = $labelWidth + ($col * $contactTileSize)
            $dest = [Drawing.Rectangle]::new($x, $y, $contactTileSize, $contactTileSize)
            $grass = if ($grassAssets.Count -gt 0) { $grassAssets[(Get-TilesetStableIndex $col $row 977 $grassAssets.Count)] } else { $null }
            Draw-TilesetAsset $contact.Graphics $grass $dest
            $asset = $generatedAssets | Where-Object { $_.mask -eq $mask -and $_.variant -eq $variant } | Select-Object -First 1
            $bitmap = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $asset.path).Path)
            try {
                $contact.Graphics.DrawImage($bitmap, $dest, 0, 0, $bitmap.Width, $bitmap.Height, [Drawing.GraphicsUnit]::Pixel)
            }
            finally {
                $bitmap.Dispose()
            }
            Draw-Label $contact.Graphics "v$($variant.ToString("000"))" $x ($y + $contactTileSize) $contactTileSize
        }
    }
    $contact.Bitmap.Save($contactPath, [Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $contact.Graphics.Dispose()
    $contact.Bitmap.Dispose()
}

$reportPath = Join-Path $resolvedOutputDirectory "way_dirt_road_on_grass_open_composite_masks_v010_v013_review.json"
Write-CleanJsonFile $reportPath ([pscustomobject]@{
    schema_version = 1
    generated_at_local = (Get-Date).ToString("yyyy-MM-ddTHH:mm:ssK")
    work_order_id = $WorkOrderId
    status = "candidate_only_composite_route_masks"
    masks = @($masks | ForEach-Object { Format-MaskToken $_ })
    variants = $variants
    generated_asset_count = $generatedAssets.Count
    generated_assets = @($generatedAssets | ForEach-Object {
        [pscustomobject]@{
            id = $_.id
            source_path = $_.source_path
            mask = $_.mask_token
            variant = $_.variant
            directions = $_.directions
        }
    })
    evidence = [pscustomobject]@{
        contact_sheet = ConvertTo-TilesetRelativePath $contactPath
        anchor_report = ConvertTo-TilesetRelativePath $anchorReportPath
    }
    open_acceptance_holds = @(
        "candidate_only",
        "app_scale_contrast_still_needs_human_review",
        "mask255_starburst_still_open",
        "full_route_review_must_be_refreshed_before_any_acceptance"
    )
})

Write-Host "Generated road-on-grass composite route masks:"
Write-Host "  Assets:        $($generatedAssets.Count)"
Write-Host "  Contact sheet: $contactPath"
Write-Host "  Review:        $reportPath"
Write-Host "  Anchor report: $anchorReportPath"

param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$OutputDirectory = "",
    [string]$WorkOrderId = "fn_v2_way_stone_road_on_stone_paved",
    [string]$WayOwner = "way_stone_road",
    [string]$GroundOwner = "stone_paved",
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

$waySlug = "way_stone_road_on_stone_paved"
$wayDirectory = Join-Path $resolvedSourceRoot "way_segment/$waySlug"
New-Item -ItemType Directory -Force -Path $wayDirectory | Out-Null

function Write-CleanJsonFile([string]$Path, $Value) {
    $json = $Value | ConvertTo-Json -Depth 24
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

function Format-VariantToken([int]$Variant) {
    return "v$($Variant.ToString("000"))"
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

function Get-ExitPoint([string]$Direction, [int]$Size) {
    $center = $Size / 2.0
    $max = $Size - 1
    switch ($Direction) {
        "N" { return [Drawing.PointF]::new($center, 0) }
        "NE" { return [Drawing.PointF]::new($max, 0) }
        "E" { return [Drawing.PointF]::new($max, $center) }
        "SE" { return [Drawing.PointF]::new($max, $max) }
        "S" { return [Drawing.PointF]::new($center, $max) }
        "SW" { return [Drawing.PointF]::new(0, $max) }
        "W" { return [Drawing.PointF]::new(0, $center) }
        "NW" { return [Drawing.PointF]::new(0, 0) }
    }
    throw "Unknown direction '$Direction'."
}

function New-RoundPen([Drawing.Color]$Color, [float]$Width) {
    $pen = [Drawing.Pen]::new($Color, $Width)
    $pen.StartCap = [Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [Drawing.Drawing2D.LineCap]::Round
    $pen.LineJoin = [Drawing.Drawing2D.LineJoin]::Round
    return $pen
}

function Draw-RoadLine($Graphics, [Drawing.PointF]$Start, [Drawing.PointF]$End, [Drawing.Pen]$Pen) {
    $Graphics.DrawLine($Pen, $Start, $End)
}

function New-SyntheticStonePavedBitmap([int]$Size, [int]$Seed = 1301) {
    $bitmap = [Drawing.Bitmap]::new($Size, $Size, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.Clear([Drawing.Color]::FromArgb(255, 111, 107, 96))
        $random = [Random]::new($Seed)
        for ($y = -8; $y -lt $Size + 8; $y += 16) {
            $offset = if (([Math]::Floor($y / 16) % 2) -eq 0) { -10 } else { 0 }
            for ($x = $offset; $x -lt $Size + 12; $x += 22) {
                $shade = $random.Next(-16, 17)
                $color = [Drawing.Color]::FromArgb(
                    255,
                    [Math]::Max(0, [Math]::Min(255, 115 + $shade)),
                    [Math]::Max(0, [Math]::Min(255, 112 + $shade)),
                    [Math]::Max(0, [Math]::Min(255, 102 + $shade))
                )
                $brush = [Drawing.SolidBrush]::new($color)
                $pen = [Drawing.Pen]::new([Drawing.Color]::FromArgb(98, 58, 56, 52), 1)
                try {
                    $rect = [Drawing.Rectangle]::new($x + $random.Next(-2, 3), $y + $random.Next(-1, 2), $random.Next(17, 24), $random.Next(12, 17))
                    $graphics.FillRectangle($brush, $rect)
                    $graphics.DrawRectangle($pen, $rect)
                }
                finally {
                    $brush.Dispose()
                    $pen.Dispose()
                }
            }
        }
    }
    finally {
        $graphics.Dispose()
    }
    return $bitmap
}

function Add-StoneRoadNoise([Drawing.Bitmap]$Bitmap, [int]$Seed) {
    $random = [Random]::new($Seed)
    for ($y = 0; $y -lt $Bitmap.Height; ++$y) {
        for ($x = 0; $x -lt $Bitmap.Width; ++$x) {
            $pixel = $Bitmap.GetPixel($x, $y)
            if ($pixel.A -lt 16) {
                continue
            }
            $jitter = $random.Next(-10, 11)
            $r = [Math]::Max(0, [Math]::Min(255, $pixel.R + $jitter))
            $g = [Math]::Max(0, [Math]::Min(255, $pixel.G + $jitter))
            $b = [Math]::Max(0, [Math]::Min(255, $pixel.B + $jitter))
            $Bitmap.SetPixel($x, $y, [Drawing.Color]::FromArgb($pixel.A, $r, $g, $b))
        }
    }
}

function Add-CobbleMarks($Graphics, [Drawing.Bitmap]$Bitmap, [int]$Mask, [int]$Variant, [int]$Size) {
    $random = [Random]::new(($Mask * 4099) + ($Variant * 6151))
    $mortarPen = [Drawing.Pen]::new([Drawing.Color]::FromArgb(118, 47, 46, 42), 1)
    $highlightPen = [Drawing.Pen]::new([Drawing.Color]::FromArgb(72, 188, 184, 166), 1)
    try {
        for ($i = 0; $i -lt 190; ++$i) {
            $x = $random.Next(8, $Size - 8)
            $y = $random.Next(8, $Size - 8)
            if ($Bitmap.GetPixel($x, $y).A -lt 32) {
                continue
            }
            $length = $random.Next(5, 13)
            $horizontal = ($random.Next(0, 2) -eq 0)
            if ($horizontal) {
                $Graphics.DrawLine($mortarPen, $x - [int]($length / 2), $y, $x + [int]($length / 2), $y + $random.Next(-1, 2))
                if (($i % 4) -eq 0) {
                    $Graphics.DrawLine($highlightPen, $x - 2, $y - 1, $x + 2, $y - 1)
                }
            }
            else {
                $Graphics.DrawLine($mortarPen, $x, $y - [int]($length / 2), $x + $random.Next(-1, 2), $y + [int]($length / 2))
            }
        }
    }
    finally {
        $mortarPen.Dispose()
        $highlightPen.Dispose()
    }
}

function Draw-StoneAllExitIntersection($Graphics, [int]$Variant, [int]$Size, [Drawing.Pen]$EdgePen, [Drawing.Pen]$BodyPen, [Drawing.Pen]$LightPen) {
    $center = [Drawing.PointF]::new($Size / 2.0, $Size / 2.0)
    foreach ($direction in @("N", "E", "S", "W")) {
        Draw-RoadLine $Graphics $center (Get-ExitPoint $direction $Size) $EdgePen
        Draw-RoadLine $Graphics $center (Get-ExitPoint $direction $Size) $BodyPen
        Draw-RoadLine $Graphics $center (Get-ExitPoint $direction $Size) $LightPen
    }
    $cornerStarts = @{
        NE = [Drawing.PointF]::new(86, 42)
        SE = [Drawing.PointF]::new(86, 86)
        SW = [Drawing.PointF]::new(42, 86)
        NW = [Drawing.PointF]::new(42, 42)
    }
    foreach ($direction in @("NE", "SE", "SW", "NW")) {
        Draw-RoadLine $Graphics $cornerStarts[$direction] (Get-ExitPoint $direction $Size) $EdgePen
        Draw-RoadLine $Graphics $cornerStarts[$direction] (Get-ExitPoint $direction $Size) $BodyPen
        Draw-RoadLine $Graphics $cornerStarts[$direction] (Get-ExitPoint $direction $Size) $LightPen
    }
    $plazaBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(232, 143, 139, 126))
    $rimPen = [Drawing.Pen]::new([Drawing.Color]::FromArgb(184, 55, 54, 49), 4)
    try {
        $Graphics.FillEllipse($plazaBrush, [Drawing.Rectangle]::new(31, 31, 66, 66))
        $Graphics.DrawEllipse($rimPen, [Drawing.Rectangle]::new(31, 31, 66, 66))
    }
    finally {
        $plazaBrush.Dispose()
        $rimPen.Dispose()
    }
}

function New-StoneRoadCandidatePng([int]$Mask, [int]$Variant, [string]$OutputPath, [int]$Size) {
    $bitmap = [Drawing.Bitmap]::new($Size, $Size, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.Clear([Drawing.Color]::Transparent)

        $center = [Drawing.PointF]::new($Size / 2.0, $Size / 2.0)
        $directions = @(Get-MaskDirections $Mask)
        $widthShift = @(-1.0, 1.2, -0.4, 1.8)[$Variant - 1]
        $edgePen = New-RoundPen ([Drawing.Color]::FromArgb(205, 48, 47, 43)) ([float](42 + $widthShift))
        $bodyPen = New-RoundPen (@(
            [Drawing.Color]::FromArgb(226, 132, 128, 116),
            [Drawing.Color]::FromArgb(232, 147, 143, 130),
            [Drawing.Color]::FromArgb(226, 121, 119, 110),
            [Drawing.Color]::FromArgb(236, 154, 150, 136)
        )[$Variant - 1]) ([float](34 + $widthShift))
        $lightPen = New-RoundPen ([Drawing.Color]::FromArgb(78, 199, 196, 178)) ([float](11 + ($widthShift * 0.25)))
        try {
            if ($Mask -eq 255) {
                Draw-StoneAllExitIntersection $graphics $Variant $Size $edgePen $bodyPen $lightPen
            }
            else {
                foreach ($direction in $directions) {
                    Draw-RoadLine $graphics $center (Get-ExitPoint $direction $Size) $edgePen
                }
                foreach ($direction in $directions) {
                    Draw-RoadLine $graphics $center (Get-ExitPoint $direction $Size) $bodyPen
                }
                foreach ($direction in $directions) {
                    Draw-RoadLine $graphics $center (Get-ExitPoint $direction $Size) $lightPen
                }
            }
        }
        finally {
            $edgePen.Dispose()
            $bodyPen.Dispose()
            $lightPen.Dispose()
        }

        Add-CobbleMarks $graphics $bitmap $Mask $Variant $Size
        Add-StoneRoadNoise $bitmap (($Mask * 7331) + ($Variant * 9941))
        $bitmap.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
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
                        issue = "stone_road_anchor_missing"
                        mask = Format-MaskToken $asset.mask
                        variant = $asset.variant
                        direction = $direction
                        coverage = [Math]::Round($coverage, 3)
                    }
                }
                if ((-not $expected) -and $coverage -gt 0.18) {
                    $status = "fail"
                    $errors += [pscustomobject]@{
                        issue = "stone_road_anchor_leak"
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

function Get-Luma([Drawing.Color]$Color) {
    return (0.2126 * $Color.R) + (0.7152 * $Color.G) + (0.0722 * $Color.B)
}

function Get-RgbDistance([Drawing.Color]$Left, [Drawing.Color]$Right) {
    $dr = [double]$Left.R - [double]$Right.R
    $dg = [double]$Left.G - [double]$Right.G
    $db = [double]$Left.B - [double]$Right.B
    return [Math]::Sqrt(($dr * $dr) + ($dg * $dg) + ($db * $db))
}

function Composite-Over([Drawing.Color]$Overlay, [Drawing.Color]$Base) {
    $alpha = [double]$Overlay.A / 255.0
    $inv = 1.0 - $alpha
    return [Drawing.Color]::FromArgb(
        255,
        [int]([double]$Overlay.R * $alpha + [double]$Base.R * $inv),
        [int]([double]$Overlay.G * $alpha + [double]$Base.G * $inv),
        [int]([double]$Overlay.B * $alpha + [double]$Base.B * $inv)
    )
}

function Measure-RoadContrast([string]$RoadPath, [int]$Seed) {
    $road = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $RoadPath).Path)
    $base = New-SyntheticStonePavedBitmap $road.Width $Seed
    try {
        $sumLuma = 0.0
        $sumRgb = 0.0
        $sumAlpha = 0.0
        $count = 0
        for ($y = 0; $y -lt $road.Height; ++$y) {
            for ($x = 0; $x -lt $road.Width; ++$x) {
                $overlay = $road.GetPixel($x, $y)
                if ($overlay.A -lt 24) {
                    continue
                }
                $ground = $base.GetPixel($x, $y)
                $composited = Composite-Over $overlay $ground
                $sumLuma += [Math]::Abs((Get-Luma $composited) - (Get-Luma $ground))
                $sumRgb += Get-RgbDistance $composited $ground
                $sumAlpha += $overlay.A
                ++$count
            }
        }
        if ($count -eq 0) {
            return [pscustomobject]@{
                road_pixel_count = 0
                average_luma_delta = 0.0
                average_rgb_distance = 0.0
                average_alpha = 0.0
            }
        }
        return [pscustomobject]@{
            road_pixel_count = $count
            average_luma_delta = [Math]::Round($sumLuma / $count, 2)
            average_rgb_distance = [Math]::Round($sumRgb / $count, 2)
            average_alpha = [Math]::Round($sumAlpha / $count, 2)
        }
    }
    finally {
        $road.Dispose()
        $base.Dispose()
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

function Draw-SyntheticBase($Graphics, [Drawing.Rectangle]$Destination, [int]$Seed) {
    $base = New-SyntheticStonePavedBitmap 128 $Seed
    try {
        $Graphics.DrawImage($base, $Destination, 0, 0, $base.Width, $base.Height, [Drawing.GraphicsUnit]::Pixel)
    }
    finally {
        $base.Dispose()
    }
}

$waysPath = Join-Path $resolvedSourceRoot "ways.json"
$ways = Get-Content -LiteralPath $waysPath -Raw | ConvertFrom-Json
$wayAssets = @(ConvertTo-TilesetArray (Get-TilesetProperty $ways "assets" @()))
$workOrder = $null
foreach ($candidate in (ConvertTo-TilesetArray (Get-TilesetProperty $ways "production_work_orders" @()))) {
    if ((Get-TilesetProperty $candidate "id" "") -eq $WorkOrderId) {
        $workOrder = $candidate
        break
    }
}
if ($null -eq $workOrder) {
    throw "Could not find production work order '$WorkOrderId'."
}

$targetMasks = @()
foreach ($token in (ConvertTo-TilesetArray (Get-TilesetProperty $workOrder "required_masks_or_roles" @()))) {
    if ([string]$token -notmatch '^mask(?<mask>\d{3})$') {
        throw "Unsupported road mask token '$token' in '$WorkOrderId'."
    }
    $targetMasks += [int]$Matches.mask
}
$targetVariants = [int](Get-TilesetProperty $workOrder "target_variants" 4)

$generatedAssets = @()
foreach ($mask in $targetMasks) {
    for ($variant = 1; $variant -le $targetVariants; ++$variant) {
        $maskToken = Format-MaskToken $mask
        $variantToken = Format-VariantToken $variant
        $fileName = "fantasy_neutral__city__way_segment__$waySlug`__$maskToken`__$variantToken.png"
        $outputPath = Join-Path $wayDirectory $fileName
        New-StoneRoadCandidatePng $mask $variant $outputPath $TileSize
        $generatedAssets += [pscustomobject]@{
            id = "fantasy_neutral.city.way_segment.$waySlug.$maskToken.$variantToken"
            mask = $mask
            mask_token = $maskToken
            variant = $variant
            variant_token = $variantToken
            path = $outputPath
            source_path = ConvertTo-TilesetRelativePath $outputPath
        }
    }
}

$anchorReport = Test-GeneratedAnchorContract $generatedAssets
$anchorReportPath = Resolve-TilesetPath "build/stone_road_on_stone_paved_v001_v004_anchor_report.json"
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $anchorReportPath) | Out-Null
Write-CleanJsonFile $anchorReportPath ([pscustomobject]@{
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    work_order_id = $WorkOrderId
    masks = @($targetMasks | ForEach-Object { Format-MaskToken $_ })
    variants = @(1..$targetVariants)
    anchor_contract = "alpha-window 8-neighbor stone road overlay contract"
    thresholds = [pscustomobject]@{
        present_min_coverage = 0.22
        absent_max_coverage = 0.18
    }
    summary = $anchorReport.summary
    errors = $anchorReport.errors
    anchors = $anchorReport.anchors
})
if ($anchorReport.summary.error_count -gt 0) {
    throw "Stone-road anchor validation failed with $($anchorReport.summary.error_count) issue(s). See $anchorReportPath."
}

$existingIds = @{}
foreach ($asset in $wayAssets) {
    $existingIds[(Get-TilesetProperty $asset "id" "")] = $true
}
$newManifestAssets = @()
foreach ($generated in $generatedAssets) {
    $manifestAsset = [pscustomobject]@{
        id = $generated.id
        style = "fantasy_neutral"
        scale = "city"
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
        edge_contract = "integrated_stone_paved_anchor_contract_v001_v004"
        tags = @(
            "contextual_mask_fit_candidate",
            "integrated_way_ground",
            $GroundOwner,
            "road_overlay_shortcut",
            "mask_anchor_aligned",
            "city_scale_cobble_road",
            "candidate_only",
            "synthetic_review_background"
        )
        context = [pscustomobject]@{
            way_family = $WayOwner
            ground = $GroundOwner
            surface_interaction = "street stones align to way anchors without becoming a generic overlay stripe"
            mask_mapping_status = "stone_paved_v001_v004_anchor_contract_passed"
            review_background = "synthetic_stone_paved_base_no_manifest_base_assets_yet"
        }
        status = "candidate"
        validation = [pscustomobject]@{
            human_review = "scripted_anchor_and_synthetic_contact_review_generated_candidate_only"
            notes = "Generated as candidate-only stone-road-on-stone-paved art. No active stone_paved base tiles exist yet, so review evidence composites over a synthetic paved-stone background; acceptance requires real base art and map-scale review."
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

$contrastRecords = @()
foreach ($generated in $generatedAssets) {
    $metrics = Measure-RoadContrast $generated.path (16000 + $generated.mask + $generated.variant)
    $contrastRecords += [pscustomobject]@{
        id = $generated.id
        mask = $generated.mask_token
        variant = $generated.variant
        road_pixel_count = $metrics.road_pixel_count
        average_luma_delta = $metrics.average_luma_delta
        average_rgb_distance = $metrics.average_rgb_distance
        average_alpha = $metrics.average_alpha
    }
}
$averageLumaDelta = [Math]::Round((($contrastRecords | Measure-Object -Property average_luma_delta -Average).Average), 2)
$minLumaDelta = [Math]::Round((($contrastRecords | Measure-Object -Property average_luma_delta -Minimum).Minimum), 2)
$averageRgbDistance = [Math]::Round((($contrastRecords | Measure-Object -Property average_rgb_distance -Average).Average), 2)
$minRgbDistance = [Math]::Round((($contrastRecords | Measure-Object -Property average_rgb_distance -Minimum).Minimum), 2)
$lowestContrast = @($contrastRecords | Sort-Object average_luma_delta, average_rgb_distance | Select-Object -First 8)

$contactTileSize = 58
$labelWidth = 96
$rowHeight = $contactTileSize + 22
$contactPath = Join-Path $resolvedOutputDirectory "way_stone_road_on_stone_paved_v001_v004_contact_sheet.png"
$contact = New-TilesetReviewBitmap ($labelWidth + ($targetVariants * $contactTileSize) + 22) (32 + ($targetMasks.Count * $rowHeight) + 12)
try {
    Draw-Label $contact.Graphics "stone-road-on-stone-paved v001-v004 over synthetic base" 8 8 520
    for ($row = 0; $row -lt $targetMasks.Count; ++$row) {
        $mask = [int]$targetMasks[$row]
        $y = 32 + ($row * $rowHeight)
        Draw-Label $contact.Graphics (Format-MaskToken $mask) 8 ($y + 18) 78
        for ($variant = 1; $variant -le $targetVariants; ++$variant) {
            $x = $labelWidth + (($variant - 1) * $contactTileSize)
            $dest = [Drawing.Rectangle]::new($x, $y, $contactTileSize, $contactTileSize)
            Draw-SyntheticBase $contact.Graphics $dest (20000 + ($row * 31) + $variant)
            $asset = $generatedAssets | Where-Object { $_.mask -eq $mask -and $_.variant -eq $variant } | Select-Object -First 1
            Invoke-WithBitmap $asset.path {
                param($bitmap)
                $contact.Graphics.DrawImage($bitmap, $dest, 0, 0, $bitmap.Width, $bitmap.Height, [Drawing.GraphicsUnit]::Pixel)
            }
            Draw-Label $contact.Graphics (Format-VariantToken $variant) $x ($y + $contactTileSize) $contactTileSize
        }
    }
    $contact.Bitmap.Save($contactPath, [Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $contact.Graphics.Dispose()
    $contact.Bitmap.Dispose()
}

$tileSizes = @(24, 32, 48)
$routeMasks = @(1, 5, 4, 85, 64, 20, 16, 65, 80, 255)
$routeCols = 5
$routeRows = 2
$sectionGap = 18
$footerHeight = 92
$reviewWidth = 720
$reviewHeight = (($tileSizes | ForEach-Object { ($_ * $routeRows) + 34 } | Measure-Object -Sum).Sum) + (($tileSizes.Count - 1) * $sectionGap) + $footerHeight
$appScalePath = Join-Path $resolvedOutputDirectory "way_stone_road_on_stone_paved_app_scale_review_v001_v004.png"
$app = New-TilesetReviewBitmap $reviewWidth $reviewHeight
try {
    $yBase = 8
    Draw-Label $app.Graphics "stone-road-on-stone-paved app-scale review v001-v004" 8 $yBase 460
    $yBase += 26
    foreach ($cellSize in $tileSizes) {
        Draw-Label $app.Graphics "$cellSize px cells" 8 $yBase 100
        for ($i = 0; $i -lt $routeMasks.Count; ++$i) {
            $mask = [int]$routeMasks[$i]
            $row = [Math]::Floor($i / $routeCols)
            $col = $i % $routeCols
            $x = 118 + ($col * $cellSize)
            $y = $yBase + ($row * $cellSize)
            $dest = [Drawing.Rectangle]::new($x, $y, $cellSize, $cellSize)
            Draw-SyntheticBase $app.Graphics $dest (24000 + ($cellSize * 13) + $i)
            $variant = (($i % $targetVariants) + 1)
            $asset = $generatedAssets | Where-Object { $_.mask -eq $mask -and $_.variant -eq $variant } | Select-Object -First 1
            Invoke-WithBitmap $asset.path {
                param($bitmap)
                $app.Graphics.DrawImage($bitmap, $dest, 0, 0, $bitmap.Width, $bitmap.Height, [Drawing.GraphicsUnit]::Pixel)
            }
        }
        $yBase += ($cellSize * $routeRows) + $sectionGap + 20
    }
    Draw-Label $app.Graphics "contrast avg L=$averageLumaDelta min L=$minLumaDelta avg RGB=$averageRgbDistance min RGB=$minRgbDistance" 8 ($reviewHeight - 62) 560
    Draw-Label $app.Graphics "candidate-only: synthetic background until stone_paved base art exists" 8 ($reviewHeight - 36) 540
    $app.Bitmap.Save($appScalePath, [Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $app.Graphics.Dispose()
    $app.Bitmap.Dispose()
}

$reportPath = Join-Path $resolvedOutputDirectory "way_stone_road_on_stone_paved_v001_v004_review.json"
Write-CleanJsonFile $reportPath ([pscustomobject]@{
    schema_version = 1
    generated_at_local = (Get-Date).ToString("yyyy-MM-ddTHH:mm:ssK")
    work_order_id = $WorkOrderId
    status = "candidate_only_stone_road_on_stone_paved_v001_v004"
    review_background = "synthetic_stone_paved_base_no_manifest_base_assets_yet"
    masks = @($targetMasks | ForEach-Object { Format-MaskToken $_ })
    variants = @(1..$targetVariants)
    generated_asset_count = $generatedAssets.Count
    generated_assets = @($generatedAssets | ForEach-Object {
        [pscustomobject]@{
            id = $_.id
            source_path = $_.source_path
            mask = $_.mask_token
            variant = $_.variant
        }
    })
    contrast = [pscustomobject]@{
        average_luma_delta = $averageLumaDelta
        min_luma_delta = $minLumaDelta
        average_rgb_distance = $averageRgbDistance
        min_rgb_distance = $minRgbDistance
        lowest_contrast_assets = $lowestContrast
    }
    evidence = [pscustomobject]@{
        contact_sheet = ConvertTo-TilesetRelativePath $contactPath
        app_scale_review_sheet = ConvertTo-TilesetRelativePath $appScalePath
        anchor_report = ConvertTo-TilesetRelativePath $anchorReportPath
    }
    open_acceptance_holds = @(
        "candidate_only",
        "human_art_direction_review_required",
        "stone_paved_base_art_missing",
        "map_scale_route_review_required_before_acceptance"
    )
})

Write-Host "Generated stone-road-on-stone-paved candidates:"
Write-Host "  Assets:            $($generatedAssets.Count)"
Write-Host "  Anchor errors:     $($anchorReport.summary.error_count)"
Write-Host "  Contrast avg/min:  L $averageLumaDelta / $minLumaDelta; RGB $averageRgbDistance / $minRgbDistance"
Write-Host "  Contact sheet:     $contactPath"
Write-Host "  App-scale review:  $appScalePath"
Write-Host "  Review:            $reportPath"
Write-Host "  Anchor report:     $anchorReportPath"

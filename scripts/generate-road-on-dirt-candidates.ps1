param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$OutputDirectory = "",
    [string]$WorkOrderId = "fn_v2_way_dirt_road_on_dirt",
    [string]$WayOwner = "way_dirt_road",
    [string]$GroundOwner = "dirt",
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

$waySlug = "way_dirt_road_on_dirt"
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

function Draw-OffsetRut($Graphics, [Drawing.PointF]$Start, [Drawing.PointF]$End, [double]$Offset, [Drawing.Pen]$Pen) {
    $dx = [double]$End.X - [double]$Start.X
    $dy = [double]$End.Y - [double]$Start.Y
    $length = [Math]::Sqrt(($dx * $dx) + ($dy * $dy))
    if ($length -le 0.01) {
        return
    }
    $nx = -$dy / $length
    $ny = $dx / $length
    $a = [Drawing.PointF]::new([float]($Start.X + ($nx * $Offset)), [float]($Start.Y + ($ny * $Offset)))
    $b = [Drawing.PointF]::new([float]($End.X + ($nx * $Offset)), [float]($End.Y + ($ny * $Offset)))
    $Graphics.DrawLine($Pen, $a, $b)
}

function Draw-DirtAllExitIntersection($Graphics, [int]$Variant, [int]$Size, [Drawing.Pen]$EdgePen, [Drawing.Pen]$BodyPen, [Drawing.Pen]$DustPen, [Drawing.Pen]$RutPen) {
    $center = [Drawing.PointF]::new($Size / 2.0, $Size / 2.0)
    $directions = @("N", "E", "S", "W")
    foreach ($direction in $directions) {
        Draw-RoadLine $Graphics $center (Get-ExitPoint $direction $Size) $EdgePen
    }
    foreach ($direction in $directions) {
        Draw-RoadLine $Graphics $center (Get-ExitPoint $direction $Size) $BodyPen
    }

    $cornerStarts = @{
        NE = [Drawing.PointF]::new(86, 42)
        SE = [Drawing.PointF]::new(86, 86)
        SW = [Drawing.PointF]::new(42, 86)
        NW = [Drawing.PointF]::new(42, 42)
    }
    foreach ($direction in @("NE", "SE", "SW", "NW")) {
        Draw-RoadLine $Graphics $cornerStarts[$direction] (Get-ExitPoint $direction $Size) $EdgePen
    }
    foreach ($direction in @("NE", "SE", "SW", "NW")) {
        Draw-RoadLine $Graphics $cornerStarts[$direction] (Get-ExitPoint $direction $Size) $BodyPen
    }

    $bodyBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(184, 103, 66, 37))
    $shadowBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(96, 45, 31, 20))
    $dustBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(88, 184, 132, 76))
    try {
        $Graphics.FillEllipse($shadowBrush, [Drawing.Rectangle]::new(28, 28, 72, 72))
        $Graphics.FillEllipse($bodyBrush, [Drawing.Rectangle]::new(34, 34, 60, 60))
        $Graphics.FillRectangle($bodyBrush, [Drawing.Rectangle]::new(41, 31, 46, 66))
        $Graphics.FillRectangle($bodyBrush, [Drawing.Rectangle]::new(31, 41, 66, 46))
        $Graphics.FillEllipse($dustBrush, [Drawing.Rectangle]::new(42, 42, 44, 44))
    }
    finally {
        $bodyBrush.Dispose()
        $shadowBrush.Dispose()
        $dustBrush.Dispose()
    }

    foreach ($direction in $directions) {
        Draw-RoadLine $Graphics $center (Get-ExitPoint $direction $Size) $DustPen
        Draw-OffsetRut $Graphics $center (Get-ExitPoint $direction $Size) -7.5 $RutPen
        Draw-OffsetRut $Graphics $center (Get-ExitPoint $direction $Size) 7.5 $RutPen
    }
    foreach ($direction in @("NE", "SE", "SW", "NW")) {
        Draw-RoadLine $Graphics $cornerStarts[$direction] (Get-ExitPoint $direction $Size) $DustPen
    }

    $random = [Random]::new(255000 + ($Variant * 811))
    $markBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(98, 48, 34, 21))
    try {
        for ($i = 0; $i -lt 16; ++$i) {
            $x = $random.Next(38, 88)
            $y = $random.Next(38, 88)
            $Graphics.FillEllipse($markBrush, [Drawing.Rectangle]::new($x, $y, $random.Next(2, 5), $random.Next(1, 3)))
        }
    }
    finally {
        $markBrush.Dispose()
    }
}

function Add-RoadbedNoise([Drawing.Bitmap]$Bitmap, [int]$Seed) {
    $random = [Random]::new($Seed)
    for ($y = 0; $y -lt $Bitmap.Height; ++$y) {
        for ($x = 0; $x -lt $Bitmap.Width; ++$x) {
            $pixel = $Bitmap.GetPixel($x, $y)
            if ($pixel.A -lt 12) {
                continue
            }
            $jitter = $random.Next(-8, 9)
            $alphaJitter = $random.Next(-10, 9)
            $r = [Math]::Max(0, [Math]::Min(255, $pixel.R + $jitter))
            $g = [Math]::Max(0, [Math]::Min(255, $pixel.G + [int]($jitter * 0.75)))
            $b = [Math]::Max(0, [Math]::Min(255, $pixel.B + [int]($jitter * 0.45)))
            $a = [Math]::Max(0, [Math]::Min(255, $pixel.A + $alphaJitter))
            $Bitmap.SetPixel($x, $y, [Drawing.Color]::FromArgb($a, $r, $g, $b))
        }
    }
}

function Add-RoadbedPebbles($Graphics, [int]$Mask, [int]$Variant, [int]$Size) {
    $random = [Random]::new(($Mask * 1009) + ($Variant * 31337))
    $count = 46 + ($Variant * 5)
    for ($i = 0; $i -lt $count; ++$i) {
        $x = $random.Next(12, $Size - 12)
        $y = $random.Next(12, $Size - 12)
        $w = $random.Next(1, 4)
        $h = $random.Next(1, 3)
        $alpha = $random.Next(28, 66)
        $color = if (($i % 3) -eq 0) {
            [Drawing.Color]::FromArgb($alpha, 42, 31, 21)
        }
        else {
            [Drawing.Color]::FromArgb($alpha, 178, 126, 73)
        }
        $brush = [Drawing.SolidBrush]::new($color)
        try {
            $Graphics.FillEllipse($brush, [Drawing.Rectangle]::new($x, $y, $w, $h))
        }
        finally {
            $brush.Dispose()
        }
    }
}

function New-DirtRoadCandidatePng([int]$Mask, [int]$Variant, [string]$OutputPath, [int]$Size) {
    $bitmap = [Drawing.Bitmap]::new($Size, $Size, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.Clear([Drawing.Color]::Transparent)

        $center = [Drawing.PointF]::new($Size / 2.0, $Size / 2.0)
        $directions = @(Get-MaskDirections $Mask)
        $widthShift = @(-1.5, 1.0, -0.5, 1.8)[$Variant - 1]
        $bodyAlpha = @(152, 168, 160, 176)[$Variant - 1]
        $dustAlpha = @(82, 70, 92, 78)[$Variant - 1]
        $bodyColor = @(
            [Drawing.Color]::FromArgb($bodyAlpha, 92, 59, 34),
            [Drawing.Color]::FromArgb($bodyAlpha, 102, 66, 38),
            [Drawing.Color]::FromArgb($bodyAlpha, 84, 55, 32),
            [Drawing.Color]::FromArgb($bodyAlpha, 112, 71, 39)
        )[$Variant - 1]
        $edgeColor = [Drawing.Color]::FromArgb(108, 61, 42, 25)
        $dustColor = [Drawing.Color]::FromArgb($dustAlpha, 184, 132, 76)
        $rutColor = [Drawing.Color]::FromArgb(166, 43, 31, 20)

        $edgePen = New-RoundPen $edgeColor ([float](42 + $widthShift))
        $bodyPen = New-RoundPen $bodyColor ([float](32 + $widthShift))
        $dustPen = New-RoundPen $dustColor ([float](14 + ($widthShift * 0.35)))
        $rutPen = New-RoundPen $rutColor ([float](3.6 + ($Variant * 0.12)))
        try {
            if ($Mask -eq 255) {
                Draw-DirtAllExitIntersection $graphics $Variant $Size $edgePen $bodyPen $dustPen $rutPen
            }
            elseif ($directions.Count -eq 0) {
                $brush = [Drawing.SolidBrush]::new($bodyColor)
                try {
                    $graphics.FillEllipse($brush, [Drawing.Rectangle]::new(42, 42, 44, 44))
                }
                finally {
                    $brush.Dispose()
                }
            }
            foreach ($direction in $directions) {
                if ($Mask -eq 255) {
                    break
                }
                $exit = Get-ExitPoint $direction $Size
                Draw-RoadLine $graphics $center $exit $edgePen
            }
            foreach ($direction in $directions) {
                if ($Mask -eq 255) {
                    break
                }
                $exit = Get-ExitPoint $direction $Size
                Draw-RoadLine $graphics $center $exit $bodyPen
            }
            foreach ($direction in $directions) {
                if ($Mask -eq 255) {
                    break
                }
                $exit = Get-ExitPoint $direction $Size
                Draw-RoadLine $graphics $center $exit $dustPen
            }
            foreach ($direction in $directions) {
                if ($Mask -eq 255) {
                    break
                }
                $exit = Get-ExitPoint $direction $Size
                Draw-OffsetRut $graphics $center $exit -8.0 $rutPen
                Draw-OffsetRut $graphics $center $exit 8.0 $rutPen
            }
            Add-RoadbedPebbles $graphics $Mask $Variant $Size
        }
        finally {
            $edgePen.Dispose()
            $bodyPen.Dispose()
            $dustPen.Dispose()
            $rutPen.Dispose()
        }

        Add-RoadbedNoise $bitmap (($Mask * 8191) + ($Variant * 7919))
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
                        issue = "dirt_road_anchor_missing"
                        mask = Format-MaskToken $asset.mask
                        variant = $asset.variant
                        direction = $direction
                        coverage = [Math]::Round($coverage, 3)
                    }
                }
                if ((-not $expected) -and $coverage -gt 0.18) {
                    $status = "fail"
                    $errors += [pscustomobject]@{
                        issue = "dirt_road_anchor_leak"
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

function Measure-RoadContrast([string]$RoadPath, $BaseAsset) {
    $basePath = Get-TilesetAssetSourcePath $BaseAsset
    $road = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $RoadPath).Path)
    $base = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $basePath).Path)
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
        $fileName = "fantasy_neutral__world__way_segment__$waySlug`__$maskToken`__$variantToken.png"
        $outputPath = Join-Path $wayDirectory $fileName
        New-DirtRoadCandidatePng $mask $variant $outputPath $TileSize
        $generatedAssets += [pscustomobject]@{
            id = "fantasy_neutral.world.way_segment.$waySlug.$maskToken.$variantToken"
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
$anchorReportPath = Resolve-TilesetPath "build/road_dirt_on_dirt_v001_v004_anchor_report.json"
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $anchorReportPath) | Out-Null
Write-CleanJsonFile $anchorReportPath ([pscustomobject]@{
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    work_order_id = $WorkOrderId
    masks = @($targetMasks | ForEach-Object { Format-MaskToken $_ })
    variants = @(1..$targetVariants)
    anchor_contract = "alpha-window 8-neighbor road overlay contract"
    thresholds = [pscustomobject]@{
        present_min_coverage = 0.22
        absent_max_coverage = 0.18
    }
    summary = $anchorReport.summary
    errors = $anchorReport.errors
    anchors = $anchorReport.anchors
})
if ($anchorReport.summary.error_count -gt 0) {
    throw "Road-on-dirt anchor validation failed with $($anchorReport.summary.error_count) issue(s). See $anchorReportPath."
}

$newManifestAssets = @()
$existingIds = @{}
foreach ($asset in $wayAssets) {
    $existingIds[(Get-TilesetProperty $asset "id" "")] = $true
}
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
        edge_contract = "integrated_dirt_ground_anchor_contract_v001_v004"
        tags = @(
            "contextual_mask_fit_candidate",
            "integrated_way_ground",
            $GroundOwner,
            "road_overlay_shortcut",
            "mask_anchor_aligned",
            "rutted_compacted_roadbed",
            "candidate_only"
        )
        context = [pscustomobject]@{
            way_family = $WayOwner
            ground = $GroundOwner
            surface_interaction = "rutted compacted roadbed remains readable without a halo"
            mask_mapping_status = "dirt_on_dirt_v001_v004_anchor_contract_passed"
        }
        status = "candidate"
        validation = [pscustomobject]@{
            human_review = "scripted_anchor_and_contact_review_generated_candidate_only"
            notes = "Generated as candidate-only dirt-road-on-dirt art. The overlay uses compacted roadbed and rut marks rather than a bright halo; acceptance still requires human/art-direction review at map scale."
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
$groundAssets = @($tileAssets | Where-Object {
    (Get-TilesetProperty $_ "owner" "") -eq $GroundOwner -and
    (Get-TilesetProperty $_ "asset_class" "") -eq "base" -and
    (Get-TilesetProperty $_ "status" "") -in @("candidate", "accepted")
})
if ($groundAssets.Count -eq 0) {
    throw "No active '$GroundOwner' base assets found for review compositing."
}

$contrastRecords = @()
foreach ($generated in $generatedAssets) {
    $baseAsset = $groundAssets[(Get-TilesetStableIndex $generated.mask $generated.variant 5221 $groundAssets.Count)]
    $metrics = Measure-RoadContrast $generated.path $baseAsset
    $contrastRecords += [pscustomobject]@{
        id = $generated.id
        mask = $generated.mask_token
        variant = $generated.variant
        base_asset_id = Get-TilesetProperty $baseAsset "id" ""
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
$contactPath = Join-Path $resolvedOutputDirectory "way_dirt_road_on_dirt_v001_v004_contact_sheet.png"
$contact = New-TilesetReviewBitmap ($labelWidth + ($targetVariants * $contactTileSize) + 22) (32 + ($targetMasks.Count * $rowHeight) + 12)
try {
    Draw-Label $contact.Graphics "dirt-road-on-dirt candidates v001-v004 over active dirt base" 8 8 520
    for ($row = 0; $row -lt $targetMasks.Count; ++$row) {
        $mask = [int]$targetMasks[$row]
        $y = 32 + ($row * $rowHeight)
        Draw-Label $contact.Graphics (Format-MaskToken $mask) 8 ($y + 18) 78
        for ($variant = 1; $variant -le $targetVariants; ++$variant) {
            $x = $labelWidth + (($variant - 1) * $contactTileSize)
            $dest = [Drawing.Rectangle]::new($x, $y, $contactTileSize, $contactTileSize)
            $baseAsset = $groundAssets[(Get-TilesetStableIndex $variant $row 7211 $groundAssets.Count)]
            Draw-TilesetAsset $contact.Graphics $baseAsset $dest
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
$appScalePath = Join-Path $resolvedOutputDirectory "way_dirt_road_on_dirt_app_scale_review_v001_v004.png"
$app = New-TilesetReviewBitmap $reviewWidth $reviewHeight
try {
    $yBase = 8
    Draw-Label $app.Graphics "dirt-road-on-dirt app-scale review v001-v004" 8 $yBase 420
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
            $baseAsset = $groundAssets[(Get-TilesetStableIndex $col $row (9000 + $cellSize) $groundAssets.Count)]
            Draw-TilesetAsset $app.Graphics $baseAsset $dest
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
    Draw-Label $app.Graphics "candidate-only: no accepted runtime art until human/art-direction review" 8 ($reviewHeight - 36) 520
    $app.Bitmap.Save($appScalePath, [Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $app.Graphics.Dispose()
    $app.Bitmap.Dispose()
}

$reportPath = Join-Path $resolvedOutputDirectory "way_dirt_road_on_dirt_v001_v004_review.json"
Write-CleanJsonFile $reportPath ([pscustomobject]@{
    schema_version = 1
    generated_at_local = (Get-Date).ToString("yyyy-MM-ddTHH:mm:ssK")
    work_order_id = $WorkOrderId
    status = "candidate_only_dirt_road_on_dirt_v001_v004"
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
        "map_scale_route_review_required_before_acceptance"
    )
})

Write-Host "Generated road-on-dirt candidates:"
Write-Host "  Assets:            $($generatedAssets.Count)"
Write-Host "  Anchor errors:     $($anchorReport.summary.error_count)"
Write-Host "  Contrast avg/min:  L $averageLumaDelta / $minLumaDelta; RGB $averageRgbDistance / $minRgbDistance"
Write-Host "  Contact sheet:     $contactPath"
Write-Host "  App-scale review:  $appScalePath"
Write-Host "  Review:            $reportPath"
Write-Host "  Anchor report:     $anchorReportPath"

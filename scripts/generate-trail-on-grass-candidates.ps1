param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$OutputDirectory = "",
    [string]$WorkOrderId = "fn_v2_way_trail_narrow_on_grass_open",
    [string]$WayOwner = "way_trail_narrow",
    [string]$GroundOwner = "grass_open",
    [string]$OwningWorkstream = "",
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

$waySlug = "way_trail_narrow_on_$GroundOwner"
$artifactSlug = $waySlug
$groundLabel = $GroundOwner -replace "_", " "
$hyphenGroundLabel = $GroundOwner -replace "_", "-"
$anchorSlug = if ($GroundOwner -eq "grass_open") { "trail_on_grass" } else { "trail_on_$GroundOwner" }
if (-not $OwningWorkstream) {
    $OwningWorkstream = if ($GroundOwner -eq "dirt") { "Dirt Sand Manager" } else { "Grass Manager" }
}
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

function Get-ExitPoint([string]$Direction, [int]$Size, [int]$Inset = 0) {
    $center = $Size / 2.0
    $min = $Inset
    $max = $Size - 1 - $Inset
    switch ($Direction) {
        "N" { return [Drawing.PointF]::new($center, $min) }
        "NE" { return [Drawing.PointF]::new($max, $min) }
        "E" { return [Drawing.PointF]::new($max, $center) }
        "SE" { return [Drawing.PointF]::new($max, $max) }
        "S" { return [Drawing.PointF]::new($center, $max) }
        "SW" { return [Drawing.PointF]::new($min, $max) }
        "W" { return [Drawing.PointF]::new($min, $center) }
        "NW" { return [Drawing.PointF]::new($min, $min) }
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

function Draw-TrailPebbles($Graphics, [int]$Mask, [int]$Variant, [int]$Size) {
    $random = [Random]::new(($Mask * 3001) + ($Variant * 9176))
    $brushes = @(
        [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(92, 94, 72, 43)),
        [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(70, 158, 132, 82)),
        [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(58, 65, 86, 38))
    )
    try {
        $count = 18 + ($Variant * 5)
        for ($i = 0; $i -lt $count; ++$i) {
            $x = $random.Next(18, $Size - 18)
            $y = $random.Next(18, $Size - 18)
            $w = $random.Next(1, 4)
            $h = $random.Next(1, 3)
            $Graphics.FillEllipse($brushes[$i % $brushes.Count], [Drawing.Rectangle]::new($x, $y, $w, $h))
        }
    }
    finally {
        foreach ($brush in $brushes) {
            $brush.Dispose()
        }
    }
}

function Add-TrailTexture([Drawing.Bitmap]$Bitmap, [int]$Seed) {
    $random = [Random]::new($Seed)
    for ($y = 0; $y -lt $Bitmap.Height; ++$y) {
        for ($x = 0; $x -lt $Bitmap.Width; ++$x) {
            $pixel = $Bitmap.GetPixel($x, $y)
            if ($pixel.A -lt 12) {
                continue
            }
            $jitter = $random.Next(-7, 8)
            $alphaJitter = $random.Next(-12, 8)
            $r = [Math]::Max(0, [Math]::Min(255, $pixel.R + $jitter))
            $g = [Math]::Max(0, [Math]::Min(255, $pixel.G + [int]($jitter * 0.8)))
            $b = [Math]::Max(0, [Math]::Min(255, $pixel.B + [int]($jitter * 0.5)))
            $a = [Math]::Max(0, [Math]::Min(255, $pixel.A + $alphaJitter))
            $Bitmap.SetPixel($x, $y, [Drawing.Color]::FromArgb($a, $r, $g, $b))
        }
    }
}

function New-TrailCandidatePng([int]$Mask, [int]$Variant, [string]$OutputPath, [int]$Size) {
    $bitmap = [Drawing.Bitmap]::new($Size, $Size, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.Clear([Drawing.Color]::Transparent)

        $directions = @(Get-MaskDirections $Mask)
        $center = [Drawing.PointF]::new($Size / 2.0, $Size / 2.0)
        $shadowPen = New-RoundPen ([Drawing.Color]::FromArgb(105, 54, 42, 24)) (16 + $Variant)
        $soilPen = New-RoundPen ([Drawing.Color]::FromArgb(172, 114, 83, 45)) (9 + $Variant)
        $dustPen = New-RoundPen ([Drawing.Color]::FromArgb(126, 176, 147, 88)) 3.0
        $grassCutPen = New-RoundPen ([Drawing.Color]::FromArgb(62, 56, 88, 32)) 19.0
        try {
            foreach ($direction in $directions) {
                $end = Get-ExitPoint $direction $Size 0
                $mid = [Drawing.PointF]::new(
                    [float](($center.X * 0.62) + ($end.X * 0.38) + (($Variant - 1.5) * 2.0)),
                    [float](($center.Y * 0.62) + ($end.Y * 0.38) - (($Variant - 1.5) * 1.5)))
                $path = [Drawing.Drawing2D.GraphicsPath]::new()
                try {
                    $path.AddBezier($center, $mid, $mid, $end)
                    $graphics.DrawPath($grassCutPen, $path)
                    $graphics.DrawPath($shadowPen, $path)
                    $graphics.DrawPath($soilPen, $path)
                    $graphics.DrawPath($dustPen, $path)
                }
                finally {
                    $path.Dispose()
                }
            }
        }
        finally {
            $shadowPen.Dispose()
            $soilPen.Dispose()
            $dustPen.Dispose()
            $grassCutPen.Dispose()
        }

        $centerBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(116, 128, 93, 49))
        $dustBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(80, 181, 151, 92))
        try {
            $graphics.FillEllipse($centerBrush, [Drawing.Rectangle]::new(46, 46, 36, 36))
            $graphics.FillEllipse($dustBrush, [Drawing.Rectangle]::new(52, 52, 24, 24))
        }
        finally {
            $centerBrush.Dispose()
            $dustBrush.Dispose()
        }

        Draw-TrailPebbles $graphics $Mask $Variant $Size
        Add-TrailTexture $bitmap (($Mask * 10007) + ($Variant * 113))
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
                if ($expected -and $coverage -lt 0.16) {
                    $status = "fail"
                    $errors += [pscustomobject]@{
                        issue = "trail_anchor_missing"
                        mask = Format-MaskToken $asset.mask
                        variant = $asset.variant
                        direction = $direction
                        coverage = [Math]::Round($coverage, 3)
                    }
                }
                if ((-not $expected) -and $coverage -gt 0.16) {
                    $status = "fail"
                    $errors += [pscustomobject]@{
                        issue = "trail_anchor_leak"
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

$targetMasks = @(1, 2, 4, 5, 8, 16, 17, 20, 25, 32, 34, 64, 65, 68, 76, 80, 85, 128, 136, 145, 196, 255)
$targetVariants = 2

$waysPath = Join-Path $resolvedSourceRoot "ways.json"
$ways = Get-Content -LiteralPath $waysPath -Raw | ConvertFrom-Json
if (-not @($ways.production_work_orders | Where-Object { (Get-TilesetProperty $_ "id" "") -eq $WorkOrderId }).Count) {
    $ways.production_work_orders = @($ways.production_work_orders) + [pscustomobject]@{
        id = $WorkOrderId
        asset_class = "integrated_way_ground"
        owner = $WayOwner
        scale = "world"
        status = "work_order"
        owning_workstream = $OwningWorkstream
        required_context = [pscustomobject]@{
            way_family = $WayOwner
            ground = $GroundOwner
            route_masks = @($targetMasks | ForEach-Object { Format-MaskToken $_ })
            forbidden_shortcuts = @(
                "generic_overlay_stroke",
                "road_family_collapse",
                "unanchored_path_stamp"
            )
        }
        required_masks_or_roles = @($targetMasks | ForEach-Object { Format-MaskToken $_ })
        target_variants = $targetVariants
        qa_labels = @(
            "road_overlay_shortcut",
            "style_family_fallback"
        )
    }
}

$generatedAssets = @()
foreach ($mask in $targetMasks) {
    for ($variant = 1; $variant -le $targetVariants; ++$variant) {
        $maskToken = Format-MaskToken $mask
        $variantToken = Format-VariantToken $variant
        $fileName = "fantasy_neutral__world__way_segment__$waySlug`__$maskToken`__$variantToken.png"
        $outputPath = Join-Path $wayDirectory $fileName
        New-TrailCandidatePng $mask $variant $outputPath $TileSize
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
$anchorReportPath = Resolve-TilesetPath "build/$($anchorSlug)_v001_v002_anchor_report.json"
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $anchorReportPath) | Out-Null
Write-CleanJsonFile $anchorReportPath ([pscustomobject]@{
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    work_order_id = $WorkOrderId
    masks = @($targetMasks | ForEach-Object { Format-MaskToken $_ })
    variants = @(1..$targetVariants)
    anchor_contract = "alpha-window 8-neighbor narrow trail contract"
    thresholds = [pscustomobject]@{
        present_min_coverage = 0.16
        absent_max_coverage = 0.16
    }
    summary = $anchorReport.summary
    errors = $anchorReport.errors
    anchors = $anchorReport.anchors
})
if ($anchorReport.summary.error_count -gt 0) {
    throw "Trail-on-$hyphenGroundLabel anchor validation failed with $($anchorReport.summary.error_count) issue(s). See $anchorReportPath."
}

$existingIds = @{}
foreach ($asset in (ConvertTo-TilesetArray (Get-TilesetProperty $ways "assets" @()))) {
    $existingIds[(Get-TilesetProperty $asset "id" "")] = $true
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
        edge_contract = "integrated_trail_ground_anchor_contract_v001_v002"
        tags = @(
            "contextual_mask_fit_candidate",
            "integrated_way_ground",
            $GroundOwner,
            "trail_narrow",
            "mask_anchor_aligned",
            "candidate_only"
        )
        context = [pscustomobject]@{
            way_family = $WayOwner
            ground = $GroundOwner
            surface_interaction = "narrow footpath presses into $groundLabel with broken shoulders and low-contrast dirt"
            mask_mapping_status = "$($anchorSlug)_v001_v002_anchor_contract_passed"
        }
        status = "candidate"
        validation = [pscustomobject]@{
            human_review = "scripted_anchor_and_contact_review_generated_candidate_only"
            notes = "Generated as candidate-only trail-on-$hyphenGroundLabel art to broaden road-family coverage beyond dirt and stone roads. Acceptance still requires human/art-direction review at generated map scale."
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

$contactTileSize = 54
$labelWidth = 96
$rowHeight = $contactTileSize + 22
$contactPath = Join-Path $resolvedOutputDirectory "$($artifactSlug)_v001_v002_contact_sheet.png"
$contact = New-TilesetReviewBitmap ($labelWidth + ($targetVariants * $contactTileSize) + 22) (32 + ($targetMasks.Count * $rowHeight) + 12)
try {
    Draw-Label $contact.Graphics "trail-on-$hyphenGroundLabel candidates v001-v002 over active $groundLabel base" 8 8 520
    for ($row = 0; $row -lt $targetMasks.Count; ++$row) {
        $mask = [int]$targetMasks[$row]
        $y = 32 + ($row * $rowHeight)
        Draw-Label $contact.Graphics (Format-MaskToken $mask) 8 ($y + 18) 78
        for ($variant = 1; $variant -le $targetVariants; ++$variant) {
            $x = $labelWidth + (($variant - 1) * $contactTileSize)
            $dest = [Drawing.Rectangle]::new($x, $y, $contactTileSize, $contactTileSize)
            $baseAsset = $groundAssets[(Get-TilesetStableIndex $variant $row 3431 $groundAssets.Count)]
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
$routeMasks = @(1, 17, 4, 68, 64, 34, 136, 255, 25, 196)
$routeCols = 5
$routeRows = 2
$sectionGap = 18
$reviewWidth = 720
$reviewHeight = (($tileSizes | ForEach-Object { ($_ * $routeRows) + 34 } | Measure-Object -Sum).Sum) + (($tileSizes.Count - 1) * $sectionGap) + 82
$appScalePath = Join-Path $resolvedOutputDirectory "$($artifactSlug)_app_scale_review_v001_v002.png"
$app = New-TilesetReviewBitmap $reviewWidth $reviewHeight
try {
    $yBase = 8
    Draw-Label $app.Graphics "trail-on-$hyphenGroundLabel app-scale review v001-v002" 8 $yBase 420
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
            $baseAsset = $groundAssets[(Get-TilesetStableIndex $col $row (5000 + $cellSize) $groundAssets.Count)]
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
    Draw-Label $app.Graphics "candidate-only: broadens trail family coverage, not accepted runtime art" 8 ($reviewHeight - 36) 560
    $app.Bitmap.Save($appScalePath, [Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $app.Graphics.Dispose()
    $app.Bitmap.Dispose()
}

$reportPath = Join-Path $resolvedOutputDirectory "$($artifactSlug)_v001_v002_review.json"
Write-CleanJsonFile $reportPath ([pscustomobject]@{
    schema_version = 1
    generated_at_local = (Get-Date).ToString("yyyy-MM-ddTHH:mm:ssK")
    work_order_id = $WorkOrderId
    status = "candidate_only_trail_on_$($GroundOwner)_v001_v002"
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
    evidence = [pscustomobject]@{
        contact_sheet = ConvertTo-TilesetRelativePath $contactPath
        app_scale_review_sheet = ConvertTo-TilesetRelativePath $appScalePath
        anchor_report = ConvertTo-TilesetRelativePath $anchorReportPath
    }
    open_acceptance_holds = @(
        "candidate_only",
        "human_art_direction_review_required",
        "trail_family_generated_map_review_required_before_acceptance"
    )
})

Write-Host "Generated trail-on-$hyphenGroundLabel candidates:"
Write-Host "  Assets:        $($generatedAssets.Count)"
Write-Host "  Anchor errors: $($anchorReport.summary.error_count)"
Write-Host "  Contact sheet: $contactPath"
Write-Host "  App review:    $appScalePath"
Write-Host "  Review:        $reportPath"
Write-Host "  Anchor report: $anchorReportPath"

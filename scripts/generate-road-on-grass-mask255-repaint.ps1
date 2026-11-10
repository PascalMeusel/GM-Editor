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

function New-SeededRandom([int]$Mask, [int]$Variant) {
    $seed = (([long]$Mask * 1103515245L) -bxor ([long]$Variant * 2654435761L)) -band 0x7fffffff
    return [Random]::new([int]$seed)
}

function Get-RandomJitter([Random]$Random, [double]$Magnitude) {
    return ([double]$Random.NextDouble() * ($Magnitude * 2.0)) - $Magnitude
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
        [float]($Start.X + ($dx * 0.36) + ($nx * $jitter1)),
        [float]($Start.Y + ($dy * 0.36) + ($ny * $jitter1))
    )
    $c2 = [Drawing.PointF]::new(
        [float]($Start.X + ($dx * 0.74) + ($nx * $jitter2)),
        [float]($Start.Y + ($dy * 0.74) + ($ny * $jitter2))
    )
    $Path.StartFigure()
    $Path.AddBezier($Start, $c1, $c2, $End)
}

function New-AllExitRoadPath([int]$Variant, [int]$Size) {
    $random = New-SeededRandom 255 $Variant
    $path = [Drawing.Drawing2D.GraphicsPath]::new()
    foreach ($pair in @(@("N", "S"), @("E", "W"), @("NE", "SW"), @("NW", "SE"))) {
        Add-RoadCurve $path (Get-AnchorPoint $pair[0] $Size 10) (Get-AnchorPoint $pair[1] $Size 10) $random 2.6
    }
    return $path
}

function New-CentralClearingPath([int]$Variant, [int]$Size) {
    $random = New-SeededRandom 255 ($Variant + 100)
    $center = $Size / 2.0
    $points = [System.Collections.Generic.List[Drawing.PointF]]::new()
    for ($i = 0; $i -lt 18; ++$i) {
        $angle = (2.0 * [Math]::PI * $i) / 18.0
        $radius = 56.0 + (Get-RandomJitter $random 5.5)
        $x = $center + ([Math]::Cos($angle) * $radius)
        $y = $center + ([Math]::Sin($angle) * ($radius * 0.92))
        [void]$points.Add([Drawing.PointF]::new([float]$x, [float]$y))
    }
    $path = [Drawing.Drawing2D.GraphicsPath]::new()
    $path.AddClosedCurve($points.ToArray(), 0.38)
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

function Invoke-WithBrush([Drawing.Color]$Color, [scriptblock]$Body) {
    $brush = [Drawing.SolidBrush]::new($Color)
    try {
        & $Body $brush
    }
    finally {
        $brush.Dispose()
    }
}

function Apply-RoadTexture([Drawing.Bitmap]$Bitmap, [int]$Variant) {
    $random = New-SeededRandom 255 ($Variant + 41)
    for ($y = 0; $y -lt $Bitmap.Height; ++$y) {
        for ($x = 0; $x -lt $Bitmap.Width; ++$x) {
            $pixel = $Bitmap.GetPixel($x, $y)
            if ($pixel.A -lt 18) {
                continue
            }
            $noise = $random.Next(-8, 9)
            $grain = if ((($x * 7 + $y * 3 + $Variant * 13) % 29) -eq 0) { $random.Next(-20, -4) } else { 0 }
            $r = [Math]::Min(255, [Math]::Max(0, $pixel.R + $noise + $grain))
            $g = [Math]::Min(255, [Math]::Max(0, $pixel.G + [Math]::Floor($noise * 0.7) + $grain))
            $b = [Math]::Min(255, [Math]::Max(0, $pixel.B + [Math]::Floor($noise * 0.45) + [Math]::Floor($grain * 0.55)))
            $Bitmap.SetPixel($x, $y, [Drawing.Color]::FromArgb($pixel.A, [int]$r, [int]$g, [int]$b))
        }
    }
}

function New-Mask255Png([int]$Variant, [string]$OutputPath, [int]$Size) {
    $bitmap = [Drawing.Bitmap]::new($Size, $Size, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
    $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.Clear([Drawing.Color]::Transparent)
    $roadPath = New-AllExitRoadPath $Variant $Size
    $clearingPath = New-CentralClearingPath $Variant $Size
    $toneShift = ($Variant - 15) * 3
    try {
        Invoke-WithPen ([Drawing.Color]::FromArgb(112, 90, 66, 39)) 50.0 {
            param($pen)
            $graphics.DrawPath($pen, $roadPath)
        }
        Invoke-WithPen ([Drawing.Color]::FromArgb(202, 141 + $toneShift, 101 + [Math]::Floor($toneShift * 0.55), 61)) 42.0 {
            param($pen)
            $graphics.DrawPath($pen, $roadPath)
        }
        Invoke-WithPen ([Drawing.Color]::FromArgb(244, 172 + $toneShift, 124 + [Math]::Floor($toneShift * 0.55), 73)) 32.0 {
            param($pen)
            $graphics.DrawPath($pen, $roadPath)
        }
        Invoke-WithBrush ([Drawing.Color]::FromArgb(222, 157 + $toneShift, 112 + [Math]::Floor($toneShift * 0.55), 67)) {
            param($brush)
            $graphics.FillPath($brush, $clearingPath)
        }
        Invoke-WithPen ([Drawing.Color]::FromArgb(38, 105, 74, 44)) 1.0 {
            param($pen)
            $pen.DashStyle = [Drawing.Drawing2D.DashStyle]::Dot
            $graphics.DrawPath($pen, $roadPath)
        }
    }
    finally {
        $clearingPath.Dispose()
        $roadPath.Dispose()
        $graphics.Dispose()
    }

    Apply-RoadTexture $bitmap $Variant
    try {
        $bitmap.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
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
            foreach ($direction in $directions) {
                $coverage = Get-AnchorCoverage $bitmap $direction
                $status = "ok"
                if ($coverage -lt 0.22) {
                    $status = "fail"
                    $errors += [pscustomobject]@{
                        issue = "mask255_anchor_missing"
                        mask = "mask255"
                        variant = $asset.variant
                        direction = $direction
                        coverage = [Math]::Round($coverage, 3)
                    }
                }
                $anchors += [pscustomobject]@{
                    mask = "mask255"
                    variant = $asset.variant
                    direction = $direction
                    expected_exit = $true
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

$mask = 255
$maskToken = Format-MaskToken $mask
$variants = @(14, 15, 16, 17)
$generatedAssets = @()
foreach ($variant in $variants) {
    $variantToken = "v$($variant.ToString("000"))"
    $fileName = "fantasy_neutral__world__way_segment__way_dirt_road_on_grass_open__$maskToken`__$variantToken.png"
    $path = Join-Path $wayDirectory $fileName
    New-Mask255Png $variant $path $TileSize
    $generatedAssets += [pscustomobject]@{
        mask = $mask
        mask_token = $maskToken
        variant = $variant
        variant_token = $variantToken
        id = "fantasy_neutral.world.way_segment.way_dirt_road_on_grass_open.$maskToken.$variantToken"
        path = $path
        source_path = ConvertTo-TilesetRelativePath $path
    }
}

$anchorReport = Test-GeneratedAnchorContract $generatedAssets
$anchorReportPath = Resolve-TilesetPath "build/road_grass_mask255_repaint_v014_v017_anchor_report.json"
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $anchorReportPath) | Out-Null
Write-CleanJsonFile $anchorReportPath ([pscustomobject]@{
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    work_order_id = $WorkOrderId
    mask = $maskToken
    variants = $variants
    anchor_contract = "8-neighbor alpha-window all-exit contract for mask255 intersection repaint"
    thresholds = [pscustomobject]@{
        present_min_coverage = 0.22
    }
    summary = $anchorReport.summary
    errors = $anchorReport.errors
    anchors = $anchorReport.anchors
})
if ($anchorReport.summary.error_count -gt 0) {
    throw "Mask255 repaint anchor validation failed with $($anchorReport.summary.error_count) issue(s). See $anchorReportPath."
}

$waysPath = Join-Path $resolvedSourceRoot "ways.json"
$ways = Get-Content -LiteralPath $waysPath -Raw | ConvertFrom-Json
$existingIds = @{}
foreach ($asset in (ConvertTo-TilesetArray (Get-TilesetProperty $ways "assets" @()))) {
    $existingIds[(Get-TilesetProperty $asset "id" "")] = $true
    if ((Get-TilesetProperty $asset "work_order_id" "") -eq $WorkOrderId -and
        [int](Get-TilesetProperty $asset "mask" -1) -eq 255 -and
        (Get-TilesetProperty $asset "status" "") -eq "candidate") {
        $asset.status = "needs_repaint"
        $tags = [System.Collections.Generic.List[string]]::new()
        foreach ($tag in (ConvertTo-TilesetArray (Get-TilesetProperty $asset "tags" @()))) {
            if (-not $tags.Contains([string]$tag)) { [void]$tags.Add([string]$tag) }
        }
        foreach ($tag in @("mask255_starburst_rejected", "replaced_by_v014_v017_mask255_repaint")) {
            if (-not $tags.Contains($tag)) { [void]$tags.Add($tag) }
        }
        $asset.tags = @($tags)
        $asset.validation = [pscustomobject]@{
            human_review = "mask255_starburst_replaced_by_v014_v017_candidate_repaint"
            notes = "Demoted from active candidate coverage because the all-exit mask read as a radial starburst stamp in full-set review. Retained as repaint evidence; v014-v017 provide the active candidate replacement set."
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
        mask = 255
        variant = $generated.variant
        source_path = $generated.source_path
        dimensions = [pscustomobject]@{
            width = $TileSize
            height = $TileSize
        }
        tile_shape = "square"
        transparent = $true
        edge_contract = "integrated_ground_mask255_intersection_anchor_contract_v014_v017"
        tags = @(
            "contextual_mask_fit_candidate",
            "integrated_way_ground",
            $GroundOwner,
            "road_overlay_shortcut",
            "mask_anchor_aligned",
            "mask255_intersection_repaint_candidate",
            "replaces_mask255_starburst",
            "v014_v017_mask255_repaint"
        )
        context = [pscustomobject]@{
            way_family = $WayOwner
            ground = $GroundOwner
            surface_interaction = "all-exit dirt-road intersection cuts into grass with softened shoulders and a worn central clearing"
            mask_mapping_status = "mask255_intersection_repaint_v$($generated.variant.ToString("000"))_anchor_aligned_all_8_exits"
        }
        status = "candidate"
        validation = [pscustomobject]@{
            human_review = "scripted_mask255_repaint_anchor_check_passed_candidate_only"
            notes = "Generated to replace the starburst-like active mask255 candidates with a broader worn-intersection treatment. Anchor checks passed for all eight exits, but assets remain candidate-only; app-scale contrast still needs renewed review before acceptance."
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
$contactTileSize = 80
$contactPath = Join-Path $resolvedOutputDirectory "way_dirt_road_on_grass_open_mask255_repaint_v014_v017_contact_sheet.png"
$contact = New-TilesetReviewBitmap (($variants.Count * $contactTileSize) + 22) ($contactTileSize + 58)
try {
    Draw-Label $contact.Graphics "mask255 intersection repaint candidates, v014-v017" 8 6 340
    for ($index = 0; $index -lt $variants.Count; ++$index) {
        $variant = [int]$variants[$index]
        $x = 8 + ($index * $contactTileSize)
        $dest = [Drawing.Rectangle]::new($x, 30, $contactTileSize, $contactTileSize)
        $grass = if ($grassAssets.Count -gt 0) { $grassAssets[(Get-TilesetStableIndex $index 255 1031 $grassAssets.Count)] } else { $null }
        Draw-TilesetAsset $contact.Graphics $grass $dest
        $asset = $generatedAssets | Where-Object { $_.variant -eq $variant } | Select-Object -First 1
        $bitmap = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $asset.path).Path)
        try {
            $contact.Graphics.DrawImage($bitmap, $dest, 0, 0, $bitmap.Width, $bitmap.Height, [Drawing.GraphicsUnit]::Pixel)
        }
        finally {
            $bitmap.Dispose()
        }
        Draw-Label $contact.Graphics "v$($variant.ToString("000"))" $x ($dest.Y + $contactTileSize) $contactTileSize
    }
    $contact.Bitmap.Save($contactPath, [Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $contact.Graphics.Dispose()
    $contact.Bitmap.Dispose()
}

$reportPath = Join-Path $resolvedOutputDirectory "way_dirt_road_on_grass_open_mask255_repaint_v014_v017_review.json"
Write-CleanJsonFile $reportPath ([pscustomobject]@{
    schema_version = 1
    generated_at_local = (Get-Date).ToString("yyyy-MM-ddTHH:mm:ssK")
    work_order_id = $WorkOrderId
    status = "candidate_only_mask255_repaint"
    mask = $maskToken
    variants = $variants
    generated_asset_count = $generatedAssets.Count
    demoted_active_starburst_variants = @(2, 3, 4, 5)
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
        anchor_report = ConvertTo-TilesetRelativePath $anchorReportPath
    }
    open_acceptance_holds = @(
        "candidate_only",
        "app_scale_contrast_still_needs_human_review",
        "full_route_review_must_be_refreshed_before_any_acceptance"
    )
})

Write-Host "Generated road-on-grass mask255 repaint:"
Write-Host "  Assets:        $($generatedAssets.Count)"
Write-Host "  Contact sheet: $contactPath"
Write-Host "  Review:        $reportPath"
Write-Host "  Anchor report: $anchorReportPath"

param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$OutputDirectory = "",
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

$bridgeDirectory = Join-Path $resolvedSourceRoot "bridge_span"
New-Item -ItemType Directory -Force -Path $bridgeDirectory | Out-Null

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

function New-RoundPen([Drawing.Color]$Color, [float]$Width) {
    $pen = [Drawing.Pen]::new($Color, $Width)
    $pen.StartCap = [Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [Drawing.Drawing2D.LineCap]::Round
    $pen.LineJoin = [Drawing.Drawing2D.LineJoin]::Round
    return $pen
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

function Add-TransparentNoise([Drawing.Bitmap]$Bitmap, [int]$Seed, [int]$Strength = 8) {
    $random = [Random]::new($Seed)
    for ($y = 0; $y -lt $Bitmap.Height; ++$y) {
        for ($x = 0; $x -lt $Bitmap.Width; ++$x) {
            $pixel = $Bitmap.GetPixel($x, $y)
            if ($pixel.A -lt 16) {
                continue
            }
            $jitter = $random.Next(-$Strength, $Strength + 1)
            $r = [Math]::Max(0, [Math]::Min(255, $pixel.R + $jitter))
            $g = [Math]::Max(0, [Math]::Min(255, $pixel.G + $jitter))
            $b = [Math]::Max(0, [Math]::Min(255, $pixel.B + $jitter))
            $Bitmap.SetPixel($x, $y, [Drawing.Color]::FromArgb($pixel.A, $r, $g, $b))
        }
    }
}

function Draw-WoodBridgeSpan($Graphics, [Drawing.Bitmap]$Bitmap, [string]$Role, [int]$Variant, [int]$Size) {
    $centerX = [int]($Size / 2)
    $deckWidth = 48 + (($Variant - 2) * 2)
    $left = $centerX - [int]($deckWidth / 2)
    $right = $centerX + [int]($deckWidth / 2)
    $deckColor = @(
        [Drawing.Color]::FromArgb(238, 134, 88, 46),
        [Drawing.Color]::FromArgb(238, 151, 99, 52),
        [Drawing.Color]::FromArgb(238, 121, 76, 39)
    )[$Variant - 1]
    $dark = [Drawing.Color]::FromArgb(224, 67, 42, 25)
    $rail = [Drawing.Color]::FromArgb(244, 78, 50, 29)
    $rope = [Drawing.Color]::FromArgb(210, 178, 138, 82)
    $deckBrush = [Drawing.SolidBrush]::new($deckColor)
    $shadowBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(130, 35, 24, 18))
    $darkPen = [Drawing.Pen]::new($dark, 3)
    $railPen = [Drawing.Pen]::new($rail, 5)
    $ropePen = [Drawing.Pen]::new($rope, 2)
    try {
        $Graphics.FillRectangle($shadowBrush, [Drawing.Rectangle]::new($left - 5, 0, $deckWidth + 10, $Size))
        if ($Role -in @("approach_a", "approach_b")) {
            $roadBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(160, 104, 65, 35))
            try {
                $Graphics.FillEllipse($roadBrush, [Drawing.Rectangle]::new($left - 16, -12, $deckWidth + 32, $Size + 24))
            }
            finally {
                $roadBrush.Dispose()
            }
        }
        $Graphics.FillRectangle($deckBrush, [Drawing.Rectangle]::new($left, 0, $deckWidth, $Size))

        $plankStep = if ($Role -eq "mid_02") { 19 } else { 16 }
        $offset = (($Variant * 5) + ($(if ($Role -eq "mid_02") { 8 } else { 0 }))) % $plankStep
        for ($y = -$offset; $y -lt $Size + $plankStep; $y += $plankStep) {
            $Graphics.DrawLine($darkPen, $left + 2, $y, $right - 2, $y + (($Variant % 2) * 2))
        }
        for ($x = $left + 10; $x -lt $right; $x += 14) {
            $Graphics.DrawLine([Drawing.Pen]::new([Drawing.Color]::FromArgb(76, 216, 165, 91), 1), $x, 4, $x - 3, $Size - 4)
        }

        $Graphics.DrawLine($railPen, $left - 6, 0, $left - 6, $Size)
        $Graphics.DrawLine($railPen, $right + 6, 0, $right + 6, $Size)
        $Graphics.DrawLine($ropePen, $left - 13, 0, $left - 13, $Size)
        $Graphics.DrawLine($ropePen, $right + 13, 0, $right + 13, $Size)

        if ($Role -in @("threshold_a", "threshold_b")) {
            $y = if ($Role -eq "threshold_a") { 10 } else { $Size - 24 }
            $stoneBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(228, 91, 86, 75))
            try {
                $Graphics.FillRectangle($stoneBrush, [Drawing.Rectangle]::new($left - 20, $y, $deckWidth + 40, 18))
                $Graphics.DrawRectangle($darkPen, [Drawing.Rectangle]::new($left - 20, $y, $deckWidth + 40, 18))
            }
            finally {
                $stoneBrush.Dispose()
            }
        }
        if ($Role -eq "mid_02") {
            $washBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(42, 62, 38, 24))
            try {
                $Graphics.FillRectangle($washBrush, [Drawing.Rectangle]::new($left + 4, 0, $deckWidth - 8, $Size))
            }
            finally {
                $washBrush.Dispose()
            }
            $bracePen = [Drawing.Pen]::new([Drawing.Color]::FromArgb(185, 54, 34, 21), 4)
            try {
                $Graphics.DrawLine($bracePen, $left + 4, 12, $right - 4, 58)
                $Graphics.DrawLine($bracePen, $right - 4, 70, $left + 4, 116)
            }
            finally {
                $bracePen.Dispose()
            }
        }
    }
    finally {
        $deckBrush.Dispose()
        $shadowBrush.Dispose()
        $darkPen.Dispose()
        $railPen.Dispose()
        $ropePen.Dispose()
    }
    Add-TransparentNoise $Bitmap (($Variant * 1000) + $Role.GetHashCode()) 7
}

function Draw-StoneBridgeSpan($Graphics, [Drawing.Bitmap]$Bitmap, [string]$Role, [int]$Variant, [int]$Size) {
    $centerX = [int]($Size / 2)
    $deckWidth = 54 + (($Variant - 2) * 2)
    $left = $centerX - [int]($deckWidth / 2)
    $right = $centerX + [int]($deckWidth / 2)
    $stoneColor = @(
        [Drawing.Color]::FromArgb(240, 132, 129, 119),
        [Drawing.Color]::FromArgb(240, 147, 144, 132),
        [Drawing.Color]::FromArgb(240, 119, 118, 111)
    )[$Variant - 1]
    $mortar = [Drawing.Color]::FromArgb(218, 50, 49, 45)
    $edge = [Drawing.Color]::FromArgb(232, 68, 66, 61)
    $deckBrush = [Drawing.SolidBrush]::new($stoneColor)
    $edgePen = [Drawing.Pen]::new($edge, 5)
    $mortarPen = [Drawing.Pen]::new($mortar, 2)
    try {
        if ($Role -in @("approach_a", "approach_b")) {
            $approachBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(166, 132, 128, 116))
            try {
                $Graphics.FillRectangle($approachBrush, [Drawing.Rectangle]::new($left - 18, 0, $deckWidth + 36, $Size))
            }
            finally {
                $approachBrush.Dispose()
            }
        }
        $Graphics.FillRectangle($deckBrush, [Drawing.Rectangle]::new($left, 0, $deckWidth, $Size))
        $Graphics.DrawLine($edgePen, $left - 3, 0, $left - 3, $Size)
        $Graphics.DrawLine($edgePen, $right + 3, 0, $right + 3, $Size)

        $blockH = if ($Role -eq "mid_02") { 18 } else { 16 }
        $offset = (($Variant * 7) + ($(if ($Role -eq "mid_02") { 9 } else { 0 }))) % $blockH
        for ($y = -$offset; $y -lt $Size + $blockH; $y += $blockH) {
            $Graphics.DrawLine($mortarPen, $left + 2, $y, $right - 2, $y)
            $xStep = 18
            $xOffset = if ((([int](($y + 200) / $blockH)) % 2) -eq 0) { 0 } else { 9 }
            for ($x = $left + $xOffset; $x -lt $right; $x += $xStep) {
                $Graphics.DrawLine($mortarPen, $x, $y, $x, $y + $blockH)
            }
        }
        if ($Role -in @("threshold_a", "threshold_b")) {
            $y = if ($Role -eq "threshold_a") { 4 } else { $Size - 22 }
            $capBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(242, 94, 92, 84))
            try {
                $Graphics.FillRectangle($capBrush, [Drawing.Rectangle]::new($left - 16, $y, $deckWidth + 32, 18))
                $Graphics.DrawRectangle($mortarPen, [Drawing.Rectangle]::new($left - 16, $y, $deckWidth + 32, 18))
            }
            finally {
                $capBrush.Dispose()
            }
        }
        if ($Role -eq "mid_02") {
            $washBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(44, 76, 74, 68))
            try {
                $Graphics.FillRectangle($washBrush, [Drawing.Rectangle]::new($left + 5, 0, $deckWidth - 10, $Size))
            }
            finally {
                $washBrush.Dispose()
            }
            $archPen = [Drawing.Pen]::new([Drawing.Color]::FromArgb(172, 70, 68, 62), 3)
            try {
                $Graphics.DrawArc($archPen, [Drawing.Rectangle]::new($left + 6, 28, $deckWidth - 12, 58), 180, 180)
            }
            finally {
                $archPen.Dispose()
            }
        }
    }
    finally {
        $deckBrush.Dispose()
        $edgePen.Dispose()
        $mortarPen.Dispose()
    }
    Add-TransparentNoise $Bitmap (($Variant * 2000) + $Role.GetHashCode()) 6
}

function New-BridgeSpanPng([string]$Owner, [string]$Role, [int]$Variant, [string]$OutputPath, [int]$Size) {
    $bitmap = [Drawing.Bitmap]::new($Size, $Size, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.Clear([Drawing.Color]::Transparent)
        if ($Owner -eq "bridge_stone") {
            Draw-StoneBridgeSpan $graphics $bitmap $Role $Variant $Size
        }
        else {
            Draw-WoodBridgeSpan $graphics $bitmap $Role $Variant $Size
        }
        $bitmap.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function Get-EdgeCoverage([Drawing.Bitmap]$Bitmap, [string]$Edge) {
    $center = [int]($Bitmap.Width / 2)
    $half = 24
    $depth = 8
    $rect = if ($Edge -eq "top") {
        [Drawing.Rectangle]::new($center - $half, 0, ($half * 2) + 1, $depth)
    }
    else {
        [Drawing.Rectangle]::new($center - $half, $Bitmap.Height - $depth, ($half * 2) + 1, $depth)
    }
    $total = 0
    $covered = 0
    for ($y = $rect.Y; $y -lt ($rect.Y + $rect.Height); ++$y) {
        for ($x = $rect.X; $x -lt ($rect.X + $rect.Width); ++$x) {
            ++$total
            if ($Bitmap.GetPixel($x, $y).A -ge 24) {
                ++$covered
            }
        }
    }
    return $covered / $total
}

function Test-BridgeContinuity($GeneratedAssets) {
    $errors = @()
    $edges = @()
    foreach ($asset in $GeneratedAssets) {
        $bitmap = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $asset.path).Path)
        try {
            foreach ($edge in @("top", "bottom")) {
                $coverage = Get-EdgeCoverage $bitmap $edge
                $status = "ok"
                if ($coverage -lt 0.34) {
                    $status = "fail"
                    $errors += [pscustomobject]@{
                        issue = "bridge_span_edge_gap"
                        id = $asset.id
                        role = $asset.role
                        variant = $asset.variant
                        edge = $edge
                        coverage = [Math]::Round($coverage, 3)
                    }
                }
                $edges += [pscustomobject]@{
                    id = $asset.id
                    role = $asset.role
                    variant = $asset.variant
                    edge = $edge
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
            checked_edges = $edges.Count
            error_count = $errors.Count
        }
        errors = $errors
        edges = $edges
    }
}

function Get-AverageOpaqueColor([string]$Path) {
    $bitmap = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $Path).Path)
    try {
        $sumR = 0.0
        $sumG = 0.0
        $sumB = 0.0
        $count = 0
        for ($y = 0; $y -lt $bitmap.Height; ++$y) {
            for ($x = 0; $x -lt $bitmap.Width; ++$x) {
                $p = $bitmap.GetPixel($x, $y)
                if ($p.A -lt 24) {
                    continue
                }
                $sumR += $p.R
                $sumG += $p.G
                $sumB += $p.B
                ++$count
            }
        }
        if ($count -eq 0) {
            return [pscustomobject]@{ R = 0.0; G = 0.0; B = 0.0; Count = 0 }
        }
        return [pscustomobject]@{ R = $sumR / $count; G = $sumG / $count; B = $sumB / $count; Count = $count }
    }
    finally {
        $bitmap.Dispose()
    }
}

function Get-ColorDistance($Left, $Right) {
    $dr = [double]$Left.R - [double]$Right.R
    $dg = [double]$Left.G - [double]$Right.G
    $db = [double]$Left.B - [double]$Right.B
    return [Math]::Sqrt(($dr * $dr) + ($dg * $dg) + ($db * $db))
}

function Get-MiddleDistinctness($GeneratedAssets) {
    $items = @()
    foreach ($group in ($GeneratedAssets | Group-Object work_order_id, variant)) {
        $mid1 = $group.Group | Where-Object role -eq "mid_01" | Select-Object -First 1
        $mid2 = $group.Group | Where-Object role -eq "mid_02" | Select-Object -First 1
        if ($null -eq $mid1 -or $null -eq $mid2) {
            continue
        }
        $a = Get-AverageOpaqueColor $mid1.path
        $b = Get-AverageOpaqueColor $mid2.path
        $distance = [Math]::Round((Get-ColorDistance $a $b), 2)
        $items += [pscustomobject]@{
            work_order_id = $mid1.work_order_id
            owner = $mid1.owner
            variant = $mid1.variant
            mid_01 = $mid1.id
            mid_02 = $mid2.id
            average_rgb_distance = $distance
            status = if ($distance -ge 2.5) { "ok" } else { "review" }
        }
    }
    return $items
}

function Draw-BridgeBackground($Graphics, [Drawing.Rectangle]$Destination, [string]$Owner, [string]$Role, [int]$Variant) {
    $baseColor = if ($Owner -eq "bridge_stone") {
        [Drawing.Color]::FromArgb(255, 82, 92, 96)
    }
    else {
        [Drawing.Color]::FromArgb(255, 60, 105, 117)
    }
    $bankColor = if ($Owner -eq "bridge_stone") {
        [Drawing.Color]::FromArgb(255, 119, 111, 94)
    }
    else {
        [Drawing.Color]::FromArgb(255, 84, 115, 55)
    }
    $waterBrush = [Drawing.SolidBrush]::new($baseColor)
    $bankBrush = [Drawing.SolidBrush]::new($bankColor)
    $foamPen = [Drawing.Pen]::new([Drawing.Color]::FromArgb(72, 182, 215, 221), 1)
    try {
        $Graphics.FillRectangle($waterBrush, $Destination)
        if ($Role -eq "approach_a") {
            $Graphics.FillRectangle($bankBrush, $Destination)
        }
        elseif ($Role -eq "threshold_a") {
            $Graphics.FillRectangle($bankBrush, [Drawing.Rectangle]::new($Destination.X, $Destination.Y, $Destination.Width, [int]($Destination.Height * 0.38)))
        }
        elseif ($Role -eq "threshold_b") {
            $Graphics.FillRectangle($bankBrush, [Drawing.Rectangle]::new($Destination.X, $Destination.Y + [int]($Destination.Height * 0.62), $Destination.Width, [int]($Destination.Height * 0.38)))
        }
        elseif ($Role -eq "approach_b") {
            $Graphics.FillRectangle($bankBrush, $Destination)
        }
        else {
            for ($i = 0; $i -lt 5; ++$i) {
                $y = $Destination.Y + (($i * 19 + $Variant * 7) % $Destination.Height)
                $Graphics.DrawLine($foamPen, $Destination.X + 8, $y, $Destination.X + $Destination.Width - 8, $y + 2)
            }
        }
    }
    finally {
        $waterBrush.Dispose()
        $bankBrush.Dispose()
        $foamPen.Dispose()
    }
}

$waysPath = Join-Path $resolvedSourceRoot "ways.json"
$ways = Get-Content -LiteralPath $waysPath -Raw | ConvertFrom-Json
$workOrders = @(ConvertTo-TilesetArray (Get-TilesetProperty $ways "production_work_orders" @()) | Where-Object {
    (Get-TilesetProperty $_ "asset_class" "") -eq "bridge_sequence"
})
if ($workOrders.Count -eq 0) {
    throw "No bridge_sequence production work orders found."
}

$generatedAssets = @()
foreach ($workOrder in $workOrders) {
    $workOrderId = Get-TilesetProperty $workOrder "id" ""
    $owner = Get-TilesetProperty $workOrder "owner" ""
    $scale = Get-TilesetProperty $workOrder "scale" "world"
    $roles = @(ConvertTo-TilesetArray (Get-TilesetProperty $workOrder "required_masks_or_roles" @()) | ForEach-Object { [string]$_ })
    $targetVariants = [int](Get-TilesetProperty $workOrder "target_variants" 3)
    $ownerDirectory = Join-Path $bridgeDirectory $owner
    New-Item -ItemType Directory -Force -Path $ownerDirectory | Out-Null
    for ($variant = 1; $variant -le $targetVariants; ++$variant) {
        foreach ($role in $roles) {
            $variantToken = Format-VariantToken $variant
            $fileName = "fantasy_neutral__$scale`__bridge_span__$owner`__$role`__$variantToken.png"
            $outputPath = Join-Path $ownerDirectory $fileName
            New-BridgeSpanPng $owner $role $variant $outputPath $TileSize
            $generatedAssets += [pscustomobject]@{
                id = "fantasy_neutral.$scale.bridge_span.$owner.$role.$variantToken"
                owner = $owner
                work_order_id = $workOrderId
                scale = $scale
                role = $role
                variant = $variant
                variant_token = $variantToken
                path = $outputPath
                source_path = ConvertTo-TilesetRelativePath $outputPath
            }
        }
    }
}

$continuityReport = Test-BridgeContinuity $generatedAssets
$middleDistinctness = @(Get-MiddleDistinctness $generatedAssets)
$sequenceReportPath = Resolve-TilesetPath "build/bridge_sequence_v001_v003_sequence_report.json"
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $sequenceReportPath) | Out-Null
Write-CleanJsonFile $sequenceReportPath ([pscustomobject]@{
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    work_order_ids = @($workOrders | ForEach-Object { Get-TilesetProperty $_ "id" "" })
    summary = [pscustomobject]@{
        generated_assets = $generatedAssets.Count
        continuity_errors = $continuityReport.summary.error_count
        middle_distinctness_reviews = @($middleDistinctness | Where-Object status -eq "review").Count
    }
    continuity = $continuityReport
    middle_distinctness = $middleDistinctness
})
if ($continuityReport.summary.error_count -gt 0) {
    throw "Bridge continuity validation failed with $($continuityReport.summary.error_count) issue(s). See $sequenceReportPath."
}

$existingIds = @{}
foreach ($asset in (ConvertTo-TilesetArray (Get-TilesetProperty $ways "assets" @()))) {
    $existingIds[(Get-TilesetProperty $asset "id" "")] = $true
}
$newManifestAssets = @()
foreach ($generated in $generatedAssets) {
    $workOrder = $workOrders | Where-Object { (Get-TilesetProperty $_ "id" "") -eq $generated.work_order_id } | Select-Object -First 1
    $context = Get-TilesetProperty $workOrder "required_context" $null
    $manifestAsset = [pscustomobject]@{
        id = $generated.id
        style = "fantasy_neutral"
        scale = $generated.scale
        asset_class = "bridge_span"
        owner = $generated.owner
        work_order_id = $generated.work_order_id
        role = $generated.role
        span_position = $generated.role
        variant = $generated.variant
        source_path = $generated.source_path
        dimensions = [pscustomobject]@{
            width = $TileSize
            height = $TileSize
        }
        tile_shape = "square"
        transparent = $true
        edge_contract = "bridge_sequence_vertical_edge_continuity_v001_v003"
        tags = @(
            "bridge_sequence_candidate",
            "bridge_span",
            $generated.owner,
            $generated.role,
            "candidate_only"
        )
        context = [pscustomobject]@{
            bridge_family = Get-TilesetProperty $context "bridge_family" $generated.owner
            crossing = @(ConvertTo-TilesetArray (Get-TilesetProperty $context "crossing" @()))
            approach_way = Get-TilesetProperty $context "approach_way" ""
            bank_context = @(ConvertTo-TilesetArray (Get-TilesetProperty $context "bank_context" @()))
            span_position = $generated.role
            orientation = "north_south_sequence_review"
        }
        status = "candidate"
        validation = [pscustomobject]@{
            human_review = "scripted_bridge_sequence_contact_review_generated_candidate_only"
            notes = "Generated as candidate-only bridge span sequence art. Edge-continuity checks passed, but acceptance requires human/art-direction review of approach, threshold, and middle span repetition."
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

$rolesAll = @("approach_a", "threshold_a", "mid_01", "mid_02", "threshold_b", "approach_b")
$cell = 58
$labelWidth = 116
$variantCount = 3
$familyGap = 28
$familyHeight = 28 + ($rolesAll.Count * ($cell + 22))
$contactWidth = $labelWidth + ($variantCount * $cell) + 26
$contactHeight = 16 + ($workOrders.Count * $familyHeight) + (($workOrders.Count - 1) * $familyGap)
$contactPath = Join-Path $resolvedOutputDirectory "bridge_sequence_candidates_v001_v003_contact_sheet.png"
$contact = New-TilesetReviewBitmap $contactWidth $contactHeight
try {
    $yBase = 8
    foreach ($workOrder in $workOrders) {
        $owner = Get-TilesetProperty $workOrder "owner" ""
        $workOrderId = Get-TilesetProperty $workOrder "id" ""
        Draw-Label $contact.Graphics "$workOrderId candidate spans" 8 $yBase 420
        $yBase += 28
        for ($row = 0; $row -lt $rolesAll.Count; ++$row) {
            $role = $rolesAll[$row]
            $y = $yBase + ($row * ($cell + 22))
            Draw-Label $contact.Graphics $role 8 ($y + 18) 100
            for ($variant = 1; $variant -le $variantCount; ++$variant) {
                $x = $labelWidth + (($variant - 1) * $cell)
                $dest = [Drawing.Rectangle]::new($x, $y, $cell, $cell)
                Draw-BridgeBackground $contact.Graphics $dest $owner $role $variant
                $asset = $generatedAssets | Where-Object {
                    $_.work_order_id -eq $workOrderId -and $_.role -eq $role -and $_.variant -eq $variant
                } | Select-Object -First 1
                Invoke-WithBitmap $asset.path {
                    param($bitmap)
                    $contact.Graphics.DrawImage($bitmap, $dest, 0, 0, $bitmap.Width, $bitmap.Height, [Drawing.GraphicsUnit]::Pixel)
                }
                Draw-Label $contact.Graphics (Format-VariantToken $variant) $x ($y + $cell) $cell
            }
        }
        $yBase += ($rolesAll.Count * ($cell + 22)) + $familyGap
    }
    $contact.Bitmap.Save($contactPath, [Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $contact.Graphics.Dispose()
    $contact.Bitmap.Dispose()
}

$sequenceCell = 46
$sequenceGap = 26
$sequenceWidth = ($variantCount * $sequenceCell) + (($variantCount - 1) * $sequenceGap) + 160
$sequenceHeight = 24 + ($workOrders.Count * (($rolesAll.Count * $sequenceCell) + 48))
$sequencePath = Join-Path $resolvedOutputDirectory "bridge_sequence_candidates_v001_v003_sequence_review.png"
$sequence = New-TilesetReviewBitmap $sequenceWidth $sequenceHeight
try {
    $yBase = 8
    foreach ($workOrder in $workOrders) {
        $owner = Get-TilesetProperty $workOrder "owner" ""
        $workOrderId = Get-TilesetProperty $workOrder "id" ""
        Draw-Label $sequence.Graphics "$workOrderId vertical sequence preview" 8 $yBase 430
        $yBase += 26
        for ($variant = 1; $variant -le $variantCount; ++$variant) {
            $x = 120 + (($variant - 1) * ($sequenceCell + $sequenceGap))
            Draw-Label $sequence.Graphics (Format-VariantToken $variant) $x $yBase $sequenceCell
            for ($row = 0; $row -lt $rolesAll.Count; ++$row) {
                $role = $rolesAll[$row]
                $dest = [Drawing.Rectangle]::new($x, $yBase + 20 + ($row * $sequenceCell), $sequenceCell, $sequenceCell)
                Draw-BridgeBackground $sequence.Graphics $dest $owner $role $variant
                $asset = $generatedAssets | Where-Object {
                    $_.work_order_id -eq $workOrderId -and $_.role -eq $role -and $_.variant -eq $variant
                } | Select-Object -First 1
                Invoke-WithBitmap $asset.path {
                    param($bitmap)
                    $sequence.Graphics.DrawImage($bitmap, $dest, 0, 0, $bitmap.Width, $bitmap.Height, [Drawing.GraphicsUnit]::Pixel)
                }
            }
        }
        $yBase += ($rolesAll.Count * $sequenceCell) + 56
    }
    $sequence.Bitmap.Save($sequencePath, [Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $sequence.Graphics.Dispose()
    $sequence.Bitmap.Dispose()
}

$reportPath = Join-Path $resolvedOutputDirectory "bridge_sequence_candidates_v001_v003_review.json"
Write-CleanJsonFile $reportPath ([pscustomobject]@{
    schema_version = 1
    generated_at_local = (Get-Date).ToString("yyyy-MM-ddTHH:mm:ssK")
    status = "candidate_only_bridge_sequence_v001_v003"
    work_order_ids = @($workOrders | ForEach-Object { Get-TilesetProperty $_ "id" "" })
    roles = $rolesAll
    variants = @(1..$variantCount)
    generated_asset_count = $generatedAssets.Count
    generated_assets = @($generatedAssets | ForEach-Object {
        [pscustomobject]@{
            id = $_.id
            source_path = $_.source_path
            work_order_id = $_.work_order_id
            role = $_.role
            variant = $_.variant
        }
    })
    evidence = [pscustomobject]@{
        contact_sheet = ConvertTo-TilesetRelativePath $contactPath
        sequence_review_sheet = ConvertTo-TilesetRelativePath $sequencePath
        sequence_report = ConvertTo-TilesetRelativePath $sequenceReportPath
    }
    continuity_summary = $continuityReport.summary
    middle_distinctness = $middleDistinctness
    open_acceptance_holds = @(
        "candidate_only",
        "human_art_direction_review_required",
        "bridge_middle_repetition_review_required",
        "bridge_bank_integration_review_required"
    )
})

Write-Host "Generated bridge sequence candidates:"
Write-Host "  Assets:             $($generatedAssets.Count)"
Write-Host "  Continuity errors:  $($continuityReport.summary.error_count)"
Write-Host "  Contact sheet:      $contactPath"
Write-Host "  Sequence review:    $sequencePath"
Write-Host "  Review:             $reportPath"
Write-Host "  Sequence report:    $sequenceReportPath"

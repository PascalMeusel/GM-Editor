param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$OutputDirectory = "",
    [string]$WorkOrderId = "fn_v2_way_trail_narrow_on_grass_open",
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

$reviewTag = "map_art_v601_muted_trail_20260620"
$wayOwner = "way_trail_narrow"
$groundOwner = "grass_open"
$waySlug = "way_trail_narrow_on_grass_open"
$wayDirectory = Join-Path $resolvedSourceRoot "way_segment/$waySlug"
New-Item -ItemType Directory -Force -Path $wayDirectory | Out-Null

function Write-CleanJsonFile([string]$Path, $Value) {
    $json = $Value | ConvertTo-Json -Depth 40
    if ($json -is [array]) {
        $json = $json -join "`n"
    }
    $json = $json -replace "`r`n", "`n"
    $json = $json -replace "`r", "`n"
    $json = (($json -split "`n") | ForEach-Object { $_.TrimEnd() }) -join "`n"
    [System.IO.File]::WriteAllText($Path, $json.TrimEnd() + "`n", [System.Text.UTF8Encoding]::new($false))
}

function Set-JsonProperty($Object, [string]$Name, $Value) {
    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        $Object | Add-Member -NotePropertyName $Name -NotePropertyValue $Value
    }
    else {
        $property.Value = $Value
    }
}

function Format-MaskToken([int]$Mask) {
    return "mask$($Mask.ToString("000"))"
}

function Format-VariantToken([int]$Variant) {
    return "v$($Variant.ToString("000"))"
}

function ConvertFrom-MaskToken([string]$MaskToken) {
    if ($MaskToken -match "^mask([0-9]{3})$") {
        return [int]$Matches[1]
    }
    return $null
}

function New-Color([int]$R, [int]$G, [int]$B, [int]$A = 255) {
    return [Drawing.Color]::FromArgb($A, $R, $G, $B)
}

function Get-StableStringSeed([string]$Value) {
    $sum = 0
    foreach ($ch in $Value.ToCharArray()) {
        $sum = (($sum * 31) + [int][char]$ch) -band 0x7fffffff
    }
    return $sum
}

function Get-RandomSeed([int64]$Value) {
    $seed = $Value % 2147483647
    if ($seed -lt 0) {
        $seed += 2147483647
    }
    return [int]$seed
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

function Get-OppositeDirection([string]$Direction) {
    switch ($Direction) {
        "N" { return "S" }
        "NE" { return "SW" }
        "E" { return "W" }
        "SE" { return "NW" }
        "S" { return "N" }
        "SW" { return "NE" }
        "W" { return "E" }
        "NW" { return "SE" }
    }
    return ""
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

function Get-DirectionVector([string]$Direction) {
    switch ($Direction) {
        "N" { return @{ X = 0.0; Y = -1.0 } }
        "NE" { return @{ X = 0.707; Y = -0.707 } }
        "E" { return @{ X = 1.0; Y = 0.0 } }
        "SE" { return @{ X = 0.707; Y = 0.707 } }
        "S" { return @{ X = 0.0; Y = 1.0 } }
        "SW" { return @{ X = -0.707; Y = 0.707 } }
        "W" { return @{ X = -1.0; Y = 0.0 } }
        "NW" { return @{ X = -0.707; Y = -0.707 } }
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

function Draw-GrassBase($Graphics, [int]$Seed) {
    $Graphics.Clear((New-Color 91 136 72))
    $random = [Random]::new((Get-RandomSeed (23000 + [int64]$Seed)))
    $brush = [Drawing.SolidBrush]::new((New-Color 117 155 84 88))
    try {
        for ($i = 0; $i -lt 170; ++$i) {
            if (($i % 6) -eq 0) {
                $brush.Color = New-Color 62 103 55 72
            }
            elseif (($i % 5) -eq 0) {
                $brush.Color = New-Color 151 157 82 62
            }
            else {
                $brush.Color = New-Color 126 162 87 66
            }
            $x = $random.Next(-2, $TileSize)
            $y = $random.Next(-2, $TileSize)
            $w = $random.Next(2, 7)
            $h = $random.Next(1, 4)
            $Graphics.FillEllipse($brush, [Drawing.Rectangle]::new($x, $y, $w, $h))
        }
    }
    finally {
        $brush.Dispose()
    }
}

function New-PathCurve([Drawing.PointF]$Start, [Drawing.PointF]$End, [string]$Direction, [int]$Seed, [double]$CurveStrength) {
    $vector = Get-DirectionVector $Direction
    $perpX = -[double]$vector.Y
    $perpY = [double]$vector.X
    $jitter = ((($Seed % 37) - 18) / 18.0) * $CurveStrength
    $c1 = [Drawing.PointF]::new(
        [float]($Start.X + (($End.X - $Start.X) * 0.34) + ($perpX * $jitter)),
        [float]($Start.Y + (($End.Y - $Start.Y) * 0.34) + ($perpY * $jitter)))
    $c2 = [Drawing.PointF]::new(
        [float]($Start.X + (($End.X - $Start.X) * 0.70) - ($perpX * $jitter * 0.45)),
        [float]($Start.Y + (($End.Y - $Start.Y) * 0.70) - ($perpY * $jitter * 0.45)))
    $path = [Drawing.Drawing2D.GraphicsPath]::new()
    $path.AddBezier($Start, $c1, $c2, $End)
    return $path
}

function Draw-TrailPath($Graphics, [Drawing.Drawing2D.GraphicsPath]$Path, [Drawing.Pen[]]$Pens) {
    foreach ($pen in $Pens) {
        $Graphics.DrawPath($pen, $Path)
    }
}

function Draw-TrailNetwork($Graphics, [int]$Mask, [int]$Variant) {
    $directions = @(Get-MaskDirections $Mask)
    if ($directions.Count -eq 0) {
        return
    }

    $centerValue = $TileSize / 2.0
    $center = [Drawing.PointF]::new(
        [float]($centerValue + (($Variant % 2) - 0.5) * 1.4),
        [float]($centerValue + ((($Mask + $Variant) % 3) - 1) * 0.8))

    $pressedGrass = New-RoundPen (New-Color 68 97 50 58) 24.0
    $softShoulder = New-RoundPen (New-Color 83 91 53 74) 17.0
    $soil = New-RoundPen (New-Color 126 92 55 178) 11.0
    $dust = New-RoundPen (New-Color 158 128 78 94) 3.5
    try {
        $pens = @($pressedGrass, $softShoulder, $soil, $dust)
        if ($directions.Count -eq 2 -and (Get-OppositeDirection $directions[0]) -eq $directions[1]) {
            $start = Get-ExitPoint $directions[0] $TileSize
            $end = Get-ExitPoint $directions[1] $TileSize
            $path = New-PathCurve $start $end $directions[1] (($Mask * 97) + $Variant) 7.0
            try {
                Draw-TrailPath $Graphics $path $pens
            }
            finally {
                $path.Dispose()
            }
            return
        }

        foreach ($direction in $directions) {
            $end = Get-ExitPoint $direction $TileSize
            $path = New-PathCurve $center $end $direction (($Mask * 103) + ($Variant * 17) + (Get-StableStringSeed $direction)) 8.0
            try {
                Draw-TrailPath $Graphics $path $pens
            }
            finally {
                $path.Dispose()
            }
        }
    }
    finally {
        $dust.Dispose()
        $soil.Dispose()
        $softShoulder.Dispose()
        $pressedGrass.Dispose()
    }
}

function Draw-TrailTexture($Graphics, [int]$Mask, [int]$Variant) {
    $random = [Random]::new((Get-RandomSeed (($Mask * 5501) + ($Variant * 71))))
    $brush = [Drawing.SolidBrush]::new((New-Color 171 146 93 52))
    try {
        $count = 22 + ($Mask % 11)
        for ($i = 0; $i -lt $count; ++$i) {
            $brush.Color = if (($i % 3) -eq 0) { New-Color 95 70 42 48 } else { New-Color 172 143 86 44 }
            $w = $random.Next(1, 4)
            $h = $random.Next(1, 3)
            $Graphics.FillEllipse($brush, [Drawing.Rectangle]::new($random.Next(14, $TileSize - 14), $random.Next(14, $TileSize - 14), $w, $h))
        }
    }
    finally {
        $brush.Dispose()
    }
}

function Add-SubtlePixelNoise([Drawing.Bitmap]$Bitmap, [int]$Seed) {
    $random = [Random]::new((Get-RandomSeed $Seed))
    for ($y = 0; $y -lt $Bitmap.Height; ++$y) {
        for ($x = 0; $x -lt $Bitmap.Width; ++$x) {
            $pixel = $Bitmap.GetPixel($x, $y)
            $jitter = $random.Next(-4, 5)
            $r = [Math]::Max(0, [Math]::Min(255, $pixel.R + $jitter))
            $g = [Math]::Max(0, [Math]::Min(255, $pixel.G + [int]($jitter * 0.8)))
            $b = [Math]::Max(0, [Math]::Min(255, $pixel.B + [int]($jitter * 0.5)))
            $Bitmap.SetPixel($x, $y, [Drawing.Color]::FromArgb(255, $r, $g, $b))
        }
    }
}

function New-MutedTrailPng([int]$Mask, [int]$Variant, $BaseAsset, [string]$OutputPath) {
    $bitmap = [Drawing.Bitmap]::new($TileSize, $TileSize, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        Draw-TilesetAsset $graphics $BaseAsset ([Drawing.Rectangle]::new(0, 0, $TileSize, $TileSize))
        Draw-TrailNetwork $graphics $Mask $Variant
        Draw-TrailTexture $graphics $Mask $Variant
        $bitmap.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function Test-TrailPixel([Drawing.Color]$Pixel) {
    return $Pixel.R -ge 105 -and $Pixel.R -le 180 -and
        $Pixel.G -ge 70 -and $Pixel.G -le 138 -and
        $Pixel.B -ge 35 -and $Pixel.B -le 98 -and
        (($Pixel.R - $Pixel.G) -ge 12)
}

function Get-AnchorWindow([string]$Direction, [int]$Size) {
    $center = [int]($Size / 2)
    $half = 22
    $depth = 10
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
            if (Test-TrailPixel $Bitmap.GetPixel($x, $y)) {
                ++$covered
            }
        }
    }
    if ($total -eq 0) {
        return 0.0
    }
    return $covered / $total
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
                if ($expected -and $coverage -lt 0.065) {
                    $status = "fail"
                    $errors += [pscustomobject]@{
                        issue = "muted_trail_anchor_missing"
                        mask = Format-MaskToken $asset.mask
                        variant = $asset.variant
                        direction = $direction
                        coverage = [Math]::Round($coverage, 3)
                    }
                }
                if ((-not $expected) -and $coverage -gt 0.12) {
                    $status = "fail"
                    $errors += [pscustomobject]@{
                        issue = "muted_trail_anchor_leak"
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

function New-BoolGrid([int]$Width, [int]$Height) {
    $grid = @()
    for ($y = 0; $y -lt $Height; ++$y) {
        $row = @()
        for ($x = 0; $x -lt $Width; ++$x) {
            $row += $false
        }
        $grid += ,$row
    }
    return $grid
}

function Set-RouteCell($Grid, [int]$X, [int]$Y) {
    if ($Y -ge 0 -and $Y -lt $Grid.Count -and $X -ge 0 -and $X -lt $Grid[$Y].Count) {
        $Grid[$Y][$X] = $true
    }
}

function Test-RouteCell($Grid, [int]$X, [int]$Y) {
    return $Y -ge 0 -and $Y -lt $Grid.Count -and $X -ge 0 -and $X -lt $Grid[$Y].Count -and $Grid[$Y][$X]
}

function Get-RouteMaskForGrid($Grid, [int]$X, [int]$Y) {
    $mask = 0
    if (Test-RouteCell $Grid $X ($Y - 1)) { $mask = $mask -bor 1 }
    if (Test-RouteCell $Grid ($X + 1) ($Y - 1)) { $mask = $mask -bor 2 }
    if (Test-RouteCell $Grid ($X + 1) $Y) { $mask = $mask -bor 4 }
    if (Test-RouteCell $Grid ($X + 1) ($Y + 1)) { $mask = $mask -bor 8 }
    if (Test-RouteCell $Grid $X ($Y + 1)) { $mask = $mask -bor 16 }
    if (Test-RouteCell $Grid ($X - 1) ($Y + 1)) { $mask = $mask -bor 32 }
    if (Test-RouteCell $Grid ($X - 1) $Y) { $mask = $mask -bor 64 }
    if (Test-RouteCell $Grid ($X - 1) ($Y - 1)) { $mask = $mask -bor 128 }
    return $mask
}

function Add-RouteCells($Grid, [object[]]$Cells) {
    foreach ($cell in $Cells) {
        Set-RouteCell $Grid ([int]$cell[0]) ([int]$cell[1])
    }
}

$waysPath = Join-Path $resolvedSourceRoot "ways.json"
$ways = Get-Content -LiteralPath $waysPath -Raw | ConvertFrom-Json
$workOrder = $ways.production_work_orders | Where-Object { (Get-TilesetProperty $_ "id" "") -eq $WorkOrderId } | Select-Object -First 1
if ($null -eq $workOrder) {
    throw "Missing work order '$WorkOrderId'."
}

$targetMasks = @(
    ConvertTo-TilesetArray (Get-TilesetProperty $workOrder "required_masks_or_roles" @()) |
        ForEach-Object { ConvertFrom-MaskToken ([string]$_) } |
        Where-Object { $null -ne $_ } |
        Sort-Object
)
$targetVariants = [Math]::Max(1, [int](Get-TilesetProperty $workOrder "target_variants" 2))

$bundle = Get-TilesetV2ManifestBundle $resolvedSourceRoot
$tileAssets = @(Get-TilesetManifestAssets $bundle.Tiles)
$grassAssets = @($tileAssets | Where-Object {
    (Get-TilesetProperty $_ "owner" "") -eq $groundOwner -and
    (Get-TilesetProperty $_ "asset_class" "") -eq "base" -and
    (Get-TilesetProperty $_ "status" "") -in @("candidate", "accepted")
})
if ($grassAssets.Count -eq 0) {
    throw "No active '$groundOwner' base assets found for trail compositing."
}

$ways.assets = @($ways.assets | Where-Object {
    -not (
        (Get-TilesetProperty $_ "work_order_id" "") -eq $WorkOrderId -and
        $reviewTag -in @(ConvertTo-TilesetArray (Get-TilesetProperty $_ "tags" @()))
    )
})
foreach ($asset in (ConvertTo-TilesetArray (Get-TilesetProperty $ways "assets" @()))) {
    if ((Get-TilesetProperty $asset "work_order_id" "") -ne $WorkOrderId) {
        continue
    }
    if ((Get-TilesetProperty $asset "status" "") -ne "candidate") {
        continue
    }
    Set-JsonProperty $asset "status" "needs_repaint"
    Set-JsonProperty $asset "rejected_in_app_review" "2026-06-20 replaced by muted trail-on-grass v601-v602 candidate"
    $tags = @(ConvertTo-TilesetArray (Get-TilesetProperty $asset "tags" @()))
    if ("superseded_by_muted_trail_v601_v602" -notin $tags) {
        Set-JsonProperty $asset "tags" @($tags + "superseded_by_muted_trail_v601_v602")
    }
    $validation = Get-TilesetProperty $asset "validation" ([pscustomobject]@{})
    Set-JsonProperty $validation "human_review" "needs_repaint"
    Set-JsonProperty $validation "notes" "Held after review: active v401-v402 trail-on-grass reads too much like a hard-outlined narrow road. Superseded by muted v601-v602 candidates."
    Set-JsonProperty $asset "validation" $validation
}

$generatedAssets = @()
foreach ($mask in $targetMasks) {
    for ($variantIndex = 1; $variantIndex -le $targetVariants; ++$variantIndex) {
        $variant = 600 + $variantIndex
        $maskToken = Format-MaskToken $mask
        $variantToken = Format-VariantToken $variant
        $fileName = "fantasy_neutral__world__way_segment__$waySlug`__$maskToken`__$variantToken.png"
        $outputPath = Join-Path $wayDirectory $fileName
        $baseAsset = $grassAssets[(Get-TilesetStableIndex $mask $variant 9127 $grassAssets.Count)]
        New-MutedTrailPng $mask $variant $baseAsset $outputPath
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
$anchorReportPath = Resolve-TilesetPath "build/trail_on_grass_muted_v601_v602_anchor_report.json"
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $anchorReportPath) | Out-Null
Write-CleanJsonFile $anchorReportPath ([pscustomobject]@{
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    work_order_id = $WorkOrderId
    masks = @($targetMasks | ForEach-Object { Format-MaskToken $_ })
    variants = @(601..(600 + $targetVariants))
    anchor_contract = "color-window 8-neighbor muted narrow trail contract"
    thresholds = [pscustomobject]@{
        present_min_coverage = 0.065
        absent_max_coverage = 0.12
    }
    summary = $anchorReport.summary
    errors = $anchorReport.errors
    anchors = $anchorReport.anchors
})
if ($anchorReport.summary.error_count -gt 0) {
    throw "Muted trail-on-grass anchor validation failed with $($anchorReport.summary.error_count) issue(s). See $anchorReportPath."
}

$newManifestAssets = @()
foreach ($generated in $generatedAssets) {
    $newManifestAssets += [pscustomobject]@{
        id = $generated.id
        style = "fantasy_neutral"
        scale = "world"
        asset_class = "way_segment"
        owner = $wayOwner
        work_order_id = $WorkOrderId
        mask = $generated.mask
        variant = $generated.variant
        source_path = $generated.source_path
        dimensions = [pscustomobject]@{ width = $TileSize; height = $TileSize }
        tile_shape = "square"
        transparent = $false
        edge_contract = "map_art_v601_muted_trail_mask_anchor_contract"
        tags = @(
            "runtime_review_candidate",
            $reviewTag,
            "integrated_way_ground",
            $groundOwner,
            "mask_anchor_aligned",
            "less_cartoon_style_study",
            "candidate_only"
        )
        context = [pscustomobject]@{
            way_family = $wayOwner
            ground = $groundOwner
            surface_interaction = "muted narrow trail pressed into grass with soft shoulders and no center guide line"
            mask_mapping_status = "v601_muted_trail_anchor_contract_passed"
        }
        status = "candidate"
        validation = [pscustomobject]@{
            human_review = "needs_user_review"
            notes = "Muted trail-on-grass repaint candidate generated after v401-v402 read too much like a hard-outlined narrow road. Candidate-only until app-scale review and user acceptance."
        }
    }
}
$ways.assets = @($ways.assets) + $newManifestAssets
Write-CleanJsonFile $waysPath $ways

$contactCell = 70
$labelWidth = 92
$rowHeight = $contactCell + 22
$contactPath = Join-Path $resolvedOutputDirectory "way_trail_narrow_on_grass_open_muted_v601_v602_contact_sheet.png"
$contact = New-TilesetReviewBitmap ($labelWidth + ($targetVariants * $contactCell) + 24) (34 + ($targetMasks.Count * $rowHeight) + 12)
try {
    Draw-Label $contact.Graphics "muted trail-on-grass v601-v602" 8 8 330
    for ($row = 0; $row -lt $targetMasks.Count; ++$row) {
        $mask = [int]$targetMasks[$row]
        $y = 34 + ($row * $rowHeight)
        Draw-Label $contact.Graphics (Format-MaskToken $mask) 8 ($y + 18) 78
        for ($variantIndex = 1; $variantIndex -le $targetVariants; ++$variantIndex) {
            $variant = 600 + $variantIndex
            $x = $labelWidth + (($variantIndex - 1) * $contactCell)
            $dest = [Drawing.Rectangle]::new($x, $y, $contactCell, $contactCell)
            $asset = $generatedAssets | Where-Object { $_.mask -eq $mask -and $_.variant -eq $variant } | Select-Object -First 1
            $image = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $asset.path).Path)
            try {
                $contact.Graphics.DrawImage($image, $dest, 0, 0, $image.Width, $image.Height, [Drawing.GraphicsUnit]::Pixel)
            }
            finally {
                $image.Dispose()
            }
            Draw-Label $contact.Graphics (Format-VariantToken $variant) $x ($y + $contactCell) $contactCell
        }
    }
    $contact.Bitmap.Save($contactPath, [Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $contact.Graphics.Dispose()
    $contact.Bitmap.Dispose()
}

$routeWidth = 28
$routeHeight = 14
$routeGrid = New-BoolGrid $routeWidth $routeHeight
Add-RouteCells $routeGrid @(@(1,2), @(2,2), @(3,2), @(4,2), @(5,2), @(6,2), @(7,2), @(8,2), @(9,2))
Add-RouteCells $routeGrid @(@(13,1), @(13,2), @(13,3), @(13,4), @(13,5), @(13,6), @(13,7))
Add-RouteCells $routeGrid @(@(2,6), @(3,7), @(4,8), @(5,9), @(6,10), @(7,11))
Add-RouteCells $routeGrid @(@(25,6), @(24,7), @(23,8), @(22,9), @(21,10), @(20,11))
Add-RouteCells $routeGrid @(@(16,2), @(17,2), @(18,2), @(19,2), @(20,2), @(20,3), @(20,4), @(20,5), @(20,6))
Add-RouteCells $routeGrid @(@(10,9), @(10,10), @(10,11), @(10,12), @(11,12), @(12,12), @(13,12))

$tileSizes = @(24, 32, 48)
$maxTileSize = ($tileSizes | Measure-Object -Maximum).Maximum
$sectionGap = 28
$footerHeight = 74
$reviewWidth = [Math]::Max(($routeWidth * $maxTileSize), 720)
$reviewHeight = (($tileSizes | ForEach-Object { ($_ * $routeHeight) + 30 } | Measure-Object -Sum).Sum) + (($tileSizes.Count - 1) * $sectionGap) + $footerHeight
$appScalePath = Join-Path $resolvedOutputDirectory "way_trail_narrow_on_grass_open_muted_v601_v602_app_scale_review.png"
$app = New-TilesetReviewBitmap $reviewWidth $reviewHeight
$routeMaskUsage = @{}
$missingRouteMasks = @{}
try {
    $sectionY = 0
    foreach ($cellSize in $tileSizes) {
        Draw-Label $app.Graphics "muted trail review at $cellSize px cells" 8 ($sectionY + 6) 300
        for ($y = 0; $y -lt $routeHeight; ++$y) {
            for ($x = 0; $x -lt $routeWidth; ++$x) {
                $dest = [Drawing.Rectangle]::new($x * $cellSize, $sectionY + 30 + ($y * $cellSize), $cellSize, $cellSize)
                if (Test-RouteCell $routeGrid $x $y) {
                    $mask = Get-RouteMaskForGrid $routeGrid $x $y
                    $maskToken = Format-MaskToken $mask
                    if (-not $routeMaskUsage.ContainsKey($maskToken)) { $routeMaskUsage[$maskToken] = 0 }
                    $routeMaskUsage[$maskToken] += 1
                    $pool = @($generatedAssets | Where-Object { $_.mask -eq $mask } | Sort-Object id)
                    if ($pool.Count -eq 0) {
                        $missingRouteMasks[$maskToken] = $true
                        $brush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(150, 120, 24, 42))
                        try { $app.Graphics.FillRectangle($brush, $dest) }
                        finally { $brush.Dispose() }
                    }
                    else {
                        $asset = $pool[(Get-TilesetStableIndex $x $y (7701 + $cellSize) $pool.Count)]
                        $image = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $asset.path).Path)
                        try { $app.Graphics.DrawImage($image, $dest, 0, 0, $image.Width, $image.Height, [Drawing.GraphicsUnit]::Pixel) }
                        finally { $image.Dispose() }
                    }
                }
                else {
                    $baseAsset = $grassAssets[(Get-TilesetStableIndex $x $y (8101 + $cellSize) $grassAssets.Count)]
                    Draw-TilesetAsset $app.Graphics $baseAsset $dest
                }
            }
        }
        $sectionY += ($cellSize * $routeHeight) + 30 + $sectionGap
    }
    Draw-Label $app.Graphics "candidate-only: softer trail, no center guide line, not promoted" 8 ($reviewHeight - 42) 520
    $app.Bitmap.Save($appScalePath, [Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $app.Graphics.Dispose()
    $app.Bitmap.Dispose()
}

$reportPath = Join-Path $resolvedOutputDirectory "way_trail_narrow_on_grass_open_muted_v601_v602_review.json"
Write-CleanJsonFile $reportPath ([pscustomobject]@{
    schema_version = 1
    generated_at_local = (Get-Date).ToString("yyyy-MM-ddTHH:mm:ssK")
    work_order_id = $WorkOrderId
    status = "candidate_only_muted_trail_on_grass_v601_v602"
    generated_asset_count = $generatedAssets.Count
    replaced_active_candidate_status = "v401-v402 marked needs_repaint"
    masks = @($targetMasks | ForEach-Object { Format-MaskToken $_ })
    variants = @(601..(600 + $targetVariants))
    route_mask_usage = $routeMaskUsage
    missing_route_masks = @($missingRouteMasks.Keys | Sort-Object)
    evidence = [pscustomobject]@{
        contact_sheet = ConvertTo-TilesetRelativePath $contactPath
        app_scale_review_sheet = ConvertTo-TilesetRelativePath $appScalePath
        anchor_report = ConvertTo-TilesetRelativePath $anchorReportPath
    }
    open_acceptance_holds = @(
        "candidate_only",
        "user_art_direction_review_required",
        "runtime_promotion_not_performed"
    )
})

Write-Host "Generated muted trail-on-grass candidates:"
Write-Host "  Assets:        $($generatedAssets.Count)"
Write-Host "  Anchor errors: $($anchorReport.summary.error_count)"
Write-Host "  Contact sheet: $contactPath"
Write-Host "  App review:    $appScalePath"
Write-Host "  Review:        $reportPath"
Write-Host "  Anchor report: $anchorReportPath"

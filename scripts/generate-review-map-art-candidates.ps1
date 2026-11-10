param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$MaterialSourceImage = "",
    [string]$OutputDirectory = "",
    [int]$TileSize = 128
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")
Add-Type -AssemblyName System.Drawing

$resolvedSourceRoot = Resolve-TilesetPath $SourceRoot
if (-not $MaterialSourceImage) {
    $MaterialSourceImage = Join-Path $resolvedSourceRoot "ai_source/material_study_roads_rivers_20260531.png"
}
$resolvedMaterialSourceImage = Resolve-TilesetPath $MaterialSourceImage
if (-not (Test-Path -LiteralPath $resolvedMaterialSourceImage)) {
    throw "Missing material source image: $resolvedMaterialSourceImage"
}
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $resolvedSourceRoot "review/contextual_candidates"
}
$resolvedOutputDirectory = Resolve-TilesetPath $OutputDirectory
New-Item -ItemType Directory -Force -Path $resolvedOutputDirectory | Out-Null

function Write-CleanJsonFile([string]$Path, $Value) {
    $json = $Value | ConvertTo-Json -Depth 32
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

function New-RoundTexturePen([Drawing.TextureBrush]$Brush, [float]$Width) {
    $pen = [Drawing.Pen]::new($Brush, $Width)
    $pen.StartCap = [Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [Drawing.Drawing2D.LineCap]::Round
    $pen.LineJoin = [Drawing.Drawing2D.LineJoin]::Round
    return $pen
}

function New-MaterialTexture($Source, [Drawing.Rectangle]$Crop, [int]$Size, [int]$Variant) {
    $texture = [Drawing.Bitmap]::new($Size, $Size, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($texture)
    try {
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $offsetX = ($Variant * 23) % [Math]::Max(1, $Crop.Width - 8)
        $offsetY = ($Variant * 31) % [Math]::Max(1, $Crop.Height - 8)
        $src = [Drawing.Rectangle]::new(
            [Math]::Min($Source.Width - $Size, $Crop.X + $offsetX),
            [Math]::Min($Source.Height - $Size, $Crop.Y + $offsetY),
            [Math]::Min($Size, $Crop.Width),
            [Math]::Min($Size, $Crop.Height))
        $graphics.DrawImage($Source, [Drawing.Rectangle]::new(0, 0, $Size, $Size), $src, [Drawing.GraphicsUnit]::Pixel)
    }
    finally {
        $graphics.Dispose()
    }
    return $texture
}

function Draw-RouteBranch($Graphics, [Drawing.Pen]$Pen, [Drawing.PointF]$Center, [string]$Direction, [int]$Seed, [double]$CurveStrength = 10.0) {
    $exit = Get-ExitPoint $Direction $TileSize
    $vector = Get-DirectionVector $Direction
    $perpX = -[double]$vector.Y
    $perpY = [double]$vector.X
    $jitter = ((($Seed % 17) - 8) / 8.0) * $CurveStrength
    $c1 = [Drawing.PointF]::new(
        [float]($Center.X + ([double]$vector.X * 18.0) + ($perpX * $jitter)),
        [float]($Center.Y + ([double]$vector.Y * 18.0) + ($perpY * $jitter)))
    $c2 = [Drawing.PointF]::new(
        [float]($exit.X - ([double]$vector.X * 28.0) - ($perpX * $jitter * 0.45)),
        [float]($exit.Y - ([double]$vector.Y * 28.0) - ($perpY * $jitter * 0.45)))
    $Graphics.DrawBezier($Pen, $Center, $c1, $c2, $exit)
}

function Add-AlphaColorNoise([Drawing.Bitmap]$Bitmap, [int]$Seed, [int]$Strength, [double]$AlphaScale = 1.0) {
    $random = [Random]::new($Seed)
    for ($y = 0; $y -lt $Bitmap.Height; ++$y) {
        for ($x = 0; $x -lt $Bitmap.Width; ++$x) {
            $pixel = $Bitmap.GetPixel($x, $y)
            if ($pixel.A -lt 8) {
                continue
            }
            $jitter = $random.Next(-$Strength, $Strength + 1)
            $r = [Math]::Max(0, [Math]::Min(255, $pixel.R + $jitter))
            $g = [Math]::Max(0, [Math]::Min(255, $pixel.G + [int]($jitter * 0.85)))
            $b = [Math]::Max(0, [Math]::Min(255, $pixel.B + [int]($jitter * 0.65)))
            $a = [Math]::Max(0, [Math]::Min(255, [int]($pixel.A * $AlphaScale)))
            $Bitmap.SetPixel($x, $y, [Drawing.Color]::FromArgb($a, $r, $g, $b))
        }
    }
}

function Blend-VisiblePixels([Drawing.Bitmap]$Bitmap, [Drawing.Color]$Color, [double]$Amount, [double]$AlphaScale = 1.0) {
    $amountClamped = [Math]::Max(0.0, [Math]::Min(1.0, $Amount))
    for ($y = 0; $y -lt $Bitmap.Height; ++$y) {
        for ($x = 0; $x -lt $Bitmap.Width; ++$x) {
            $pixel = $Bitmap.GetPixel($x, $y)
            if ($pixel.A -lt 8) {
                continue
            }
            $r = [int](([double]$pixel.R * (1.0 - $amountClamped)) + ([double]$Color.R * $amountClamped))
            $g = [int](([double]$pixel.G * (1.0 - $amountClamped)) + ([double]$Color.G * $amountClamped))
            $b = [int](([double]$pixel.B * (1.0 - $amountClamped)) + ([double]$Color.B * $amountClamped))
            $a = [Math]::Max(0, [Math]::Min(255, [int]($pixel.A * $AlphaScale)))
            $Bitmap.SetPixel($x, $y, [Drawing.Color]::FromArgb($a, $r, $g, $b))
        }
    }
}

function Add-RandomMarks($Graphics, [Drawing.Bitmap]$Bitmap, [int]$Seed, [Drawing.Color]$Color, [int]$Count, [int]$MaxRadius) {
    $random = [Random]::new($Seed)
    $brush = [Drawing.SolidBrush]::new($Color)
    try {
        for ($i = 0; $i -lt $Count; ++$i) {
            $x = $random.Next(4, $Bitmap.Width - 4)
            $y = $random.Next(4, $Bitmap.Height - 4)
            if ($Bitmap.GetPixel($x, $y).A -lt 24) {
                continue
            }
            $w = $random.Next(1, $MaxRadius + 1)
            $h = $random.Next(1, $MaxRadius + 1)
            $Graphics.FillEllipse($brush, [Drawing.Rectangle]::new($x, $y, $w, $h))
        }
    }
    finally {
        $brush.Dispose()
    }
}

function New-WayCandidatePng(
    $Source,
    [int]$Mask,
    [int]$Variant,
    [string]$Kind,
    [string]$OutputPath,
    [int]$Size
) {
    $bitmap = [Drawing.Bitmap]::new($Size, $Size, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $texture = $null
    $brush = $null
    $edgeBrush = $null
    $edgePen = $null
    $bodyPen = $null
    $detailPen = $null
    try {
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.Clear([Drawing.Color]::Transparent)

        $crop = switch ($Kind) {
            "stone" { [Drawing.Rectangle]::new(760, 70, 420, 420) }
            "river" { [Drawing.Rectangle]::new(620, 750, 560, 440) }
            "trail" { [Drawing.Rectangle]::new(260, 240, 330, 500) }
            default { [Drawing.Rectangle]::new(210, 170, 360, 560) }
        }
        $texture = New-MaterialTexture $Source $crop $Size $Variant
        $brush = [Drawing.TextureBrush]::new($texture, [Drawing.Drawing2D.WrapMode]::Tile)
        $directions = @(Get-MaskDirections $Mask)
        $center = [Drawing.PointF]::new($Size / 2.0, $Size / 2.0)

        $width = switch ($Kind) {
            "river" { 68.0 + (($Variant % 2) * 2.0) }
            "trail" { 18.0 + (($Variant % 2) * 1.0) }
            "stone" { 46.0 + (($Variant % 2) * 2.0) }
            default { 44.0 + (($Variant % 3) * 1.5) }
        }
        $edgeColor = switch ($Kind) {
            "river" { [Drawing.Color]::FromArgb(122, 52, 118, 128) }
            "stone" { [Drawing.Color]::FromArgb(112, 56, 58, 52) }
            "trail" { [Drawing.Color]::FromArgb(76, 62, 49, 30) }
            default { [Drawing.Color]::FromArgb(82, 82, 61, 36) }
        }
        $detailColor = switch ($Kind) {
            "river" { [Drawing.Color]::FromArgb(205, 238, 252, 252) }
            "stone" { [Drawing.Color]::FromArgb(130, 64, 62, 56) }
            "trail" { [Drawing.Color]::FromArgb(92, 64, 48, 28) }
            default { [Drawing.Color]::FromArgb(96, 72, 49, 27) }
        }

        $edgeBrush = [Drawing.SolidBrush]::new($edgeColor)
        $edgePen = New-RoundPen $edgeColor ([float]($width + 11.0))
        $bodyPen = New-RoundTexturePen $brush ([float]$width)
        $detailWidth = 2.8
        if ($Kind -eq "river") {
            $detailWidth = 3.0
        }
        elseif ($Kind -eq "trail") {
            $detailWidth = 2.0
        }
        $detailPen = New-RoundPen $detailColor ([float]$detailWidth)

        if ($directions.Count -eq 0) {
            $graphics.FillEllipse($edgeBrush, [Drawing.Rectangle]::new(43, 43, 42, 42))
            $graphics.FillEllipse($brush, [Drawing.Rectangle]::new(47, 47, 34, 34))
        }
        else {
            foreach ($direction in $directions) {
                Draw-RouteBranch $graphics $edgePen $center $direction (($Mask * 101) + ($Variant * 17) + $direction.Length) 8.0
            }
            $hubScale = if ($directions.Count -ge 5) { 1.25 } elseif ($directions.Count -ge 3) { 0.98 } else { 0.68 }
            $edgeHub = [int](($width + 11.0) * $hubScale)
            $graphics.FillEllipse(
                $edgeBrush,
                [Drawing.Rectangle]::new([int]($center.X - ($edgeHub / 2.0)), [int]($center.Y - ($edgeHub / 2.0)), $edgeHub, $edgeHub))
            foreach ($direction in $directions) {
                Draw-RouteBranch $graphics $bodyPen $center $direction (($Mask * 131) + ($Variant * 19) + $direction.Length) 8.0
            }
            $bodyHub = [int]($width * $hubScale)
            $graphics.FillEllipse(
                $brush,
                [Drawing.Rectangle]::new([int]($center.X - ($bodyHub / 2.0)), [int]($center.Y - ($bodyHub / 2.0)), $bodyHub, $bodyHub))
        }

        if ($Kind -eq "stone") {
            Blend-VisiblePixels $bitmap ([Drawing.Color]::FromArgb(112, 105, 88)) 0.22 0.88
            Add-RandomMarks $graphics $bitmap (($Mask * 409) + $Variant) ([Drawing.Color]::FromArgb(92, 235, 232, 210)) 54 4
            Add-RandomMarks $graphics $bitmap (($Mask * 421) + $Variant) ([Drawing.Color]::FromArgb(96, 45, 44, 40)) 42 3
            Add-AlphaColorNoise $bitmap (($Mask * 427) + ($Variant * 983)) 4 0.92
        }
        elseif ($Kind -eq "river") {
            Blend-VisiblePixels $bitmap ([Drawing.Color]::FromArgb(56, 136, 150)) 0.32 0.82
            Add-RandomMarks $graphics $bitmap (($Mask * 433) + $Variant) ([Drawing.Color]::FromArgb(118, 226, 248, 255)) 28 5
            Add-RandomMarks $graphics $bitmap (($Mask * 437) + $Variant) ([Drawing.Color]::FromArgb(70, 245, 255, 255)) 24 2
            Add-AlphaColorNoise $bitmap (($Mask * 439) + ($Variant * 997)) 5 0.90
        }
        elseif ($Kind -eq "trail") {
            Blend-VisiblePixels $bitmap ([Drawing.Color]::FromArgb(118, 84, 45)) 0.26 0.78
            Add-RandomMarks $graphics $bitmap (($Mask * 449) + $Variant) ([Drawing.Color]::FromArgb(80, 49, 34, 19)) 34 3
            Add-RandomMarks $graphics $bitmap (($Mask * 457) + $Variant) ([Drawing.Color]::FromArgb(62, 204, 164, 98)) 24 2
            Add-AlphaColorNoise $bitmap (($Mask * 463) + ($Variant * 991)) 7 0.95
        }
        else {
            Blend-VisiblePixels $bitmap ([Drawing.Color]::FromArgb(151, 111, 60)) 0.34 0.86
            Add-RandomMarks $graphics $bitmap (($Mask * 449) + $Variant) ([Drawing.Color]::FromArgb(92, 48, 31, 18)) 44 4
            Add-RandomMarks $graphics $bitmap (($Mask * 457) + $Variant) ([Drawing.Color]::FromArgb(78, 205, 164, 98)) 34 3
            Add-AlphaColorNoise $bitmap (($Mask * 463) + ($Variant * 991)) 7 0.96
        }

        $bitmap.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        if ($detailPen) { $detailPen.Dispose() }
        if ($bodyPen) { $bodyPen.Dispose() }
        if ($edgePen) { $edgePen.Dispose() }
        if ($edgeBrush) { $edgeBrush.Dispose() }
        if ($brush) { $brush.Dispose() }
        if ($texture) { $texture.Dispose() }
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function New-StructureCandidatePng($Source, $Context, [string]$Role, [int]$Variant, [string]$OutputPath, [int]$Size) {
    $bitmap = [Drawing.Bitmap]::new($Size, $Size, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $groundTexture = $null
    $groundBrush = $null
    try {
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.Clear([Drawing.Color]::Transparent)

        $ground = [string](Get-TilesetProperty $Context "ground" "grass_open")
        $crop = switch ($ground) {
            "stone_paved" { [Drawing.Rectangle]::new(760, 70, 420, 420) }
            "dirt" { [Drawing.Rectangle]::new(170, 120, 420, 520) }
            "cave_floor" { [Drawing.Rectangle]::new(450, 190, 330, 390) }
            "dungeon_floor_stone" { [Drawing.Rectangle]::new(760, 70, 420, 420) }
            default { [Drawing.Rectangle]::new(430, 170, 350, 430) }
        }
        $groundTexture = New-MaterialTexture $Source $crop $Size $Variant
        $groundBrush = [Drawing.TextureBrush]::new($groundTexture, [Drawing.Drawing2D.WrapMode]::Tile)
        $groundPath = [Drawing.Drawing2D.GraphicsPath]::new()
        $groundPath.AddEllipse([Drawing.Rectangle]::new(8, 10, 112, 110))
        $graphics.FillPath($groundBrush, $groundPath)
        $groundPath.Dispose()

        $entrance = [string](Get-TilesetProperty $Context "entrance_type" "none")
        $orientation = [string](Get-TilesetProperty $Context "orientation" "none")
        if ($entrance -ne "none" -and $orientation -ne "none") {
            $mask = switch ($orientation) {
                "n" { 1 }
                "e" { 4 }
                "s" { 16 }
                "w" { 64 }
                default { 0 }
            }
            $kind = if ($entrance -eq "way_stone_road" -or $entrance -eq "dungeon_corridor") { "stone" } elseif ($entrance -eq "way_wood_bridge") { "trail" } elseif ($entrance -eq "cave_tunnel") { "dirt" } else { "dirt" }
            $tempPath = Join-Path ([System.IO.Path]::GetTempPath()) "structure_approach_$([Guid]::NewGuid().ToString('N')).png"
            New-WayCandidatePng $Source $mask $Variant $kind $tempPath $Size
            $approach = [Drawing.Bitmap]::new($tempPath)
            try {
                $graphics.DrawImage($approach, [Drawing.Rectangle]::new(0, 0, $Size, $Size), 0, 0, $Size, $Size, [Drawing.GraphicsUnit]::Pixel)
            }
            finally {
                $approach.Dispose()
                Remove-Item -LiteralPath $tempPath -Force -ErrorAction SilentlyContinue
            }
        }

        $shadowBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(90, 28, 24, 20))
        $stoneBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(238, 112, 110, 101))
        $stoneLight = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(238, 162, 158, 142))
        $roofBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(238, 112, 72, 38))
        $darkPen = [Drawing.Pen]::new([Drawing.Color]::FromArgb(218, 45, 40, 34), 3)
        try {
            $graphics.FillEllipse($shadowBrush, [Drawing.Rectangle]::new(34, 66, 60, 28))
            $graphics.FillEllipse($stoneBrush, [Drawing.Rectangle]::new(37, 38, 54, 50))
            $graphics.FillEllipse($stoneLight, [Drawing.Rectangle]::new(43, 33, 42, 27))
            $roofPoints = @(
                [Drawing.Point]::new(64, 16),
                [Drawing.Point]::new(92, 46),
                [Drawing.Point]::new(77, 58),
                [Drawing.Point]::new(50, 58),
                [Drawing.Point]::new(36, 46)
            )
            $graphics.FillPolygon($roofBrush, $roofPoints)
            $graphics.DrawPolygon($darkPen, $roofPoints)
            $graphics.DrawEllipse($darkPen, [Drawing.Rectangle]::new(37, 38, 54, 50))
            $doorBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(230, 58, 38, 23))
            try {
                $graphics.FillRectangle($doorBrush, [Drawing.Rectangle]::new(58, 73, 13, 16))
            }
            finally {
                $doorBrush.Dispose()
            }
        }
        finally {
            $shadowBrush.Dispose()
            $stoneBrush.Dispose()
            $stoneLight.Dispose()
            $roofBrush.Dispose()
            $darkPen.Dispose()
        }

        Add-AlphaColorNoise $bitmap (($Variant * 601) + $Role.Length) 4 1.0
        $bitmap.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        if ($groundBrush) { $groundBrush.Dispose() }
        if ($groundTexture) { $groundTexture.Dispose() }
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function Demote-ExistingCandidates($Assets, [string[]]$WorkOrderIds) {
    return @($Assets | ForEach-Object {
        $asset = $_
        $workOrder = Get-TilesetProperty $asset "work_order_id" ""
        $status = Get-TilesetProperty $asset "status" ""
        $tags = @(ConvertTo-TilesetArray (Get-TilesetProperty $asset "tags" @()))
        if ($workOrder -in $WorkOrderIds -and $status -eq "candidate" -and "material_study_20260531" -notin $tags) {
            $asset.status = "needs_repaint"
            $asset | Add-Member -Force -NotePropertyName "rejected_in_app_review" -NotePropertyValue "2026-05-31 replaced by material-study review candidates"
            $validation = Get-TilesetProperty $asset "validation" ([pscustomobject]@{})
            $validation | Add-Member -Force -NotePropertyName "replacement_note" -NotePropertyValue "Superseded by v101+ review candidates generated from material_study_roads_rivers_20260531.png."
            $asset.validation = $validation
        }
        $asset
    })
}

function Remove-MaterialStudyAssets($Assets, [string[]]$WorkOrderIds) {
    return @($Assets | Where-Object {
        $workOrder = Get-TilesetProperty $_ "work_order_id" ""
        $tags = @(ConvertTo-TilesetArray (Get-TilesetProperty $_ "tags" @()))
        -not ($workOrder -in $WorkOrderIds -and "material_study_20260531" -in $tags)
    })
}

function Ensure-RiverWorkOrder($Ways, [object[]]$ReferenceMasks) {
    $existing = @($Ways.production_work_orders | Where-Object { (Get-TilesetProperty $_ "id" "") -eq "fn_v2_way_river_stream_on_water_shallow" })
    if ($existing.Count -gt 0) {
        $existing[0].qa_labels = @("style_family_fallback")
        return
    }
    $Ways.production_work_orders = @($Ways.production_work_orders) + [pscustomobject]@{
        id = "fn_v2_way_river_stream_on_water_shallow"
        asset_class = "integrated_way_ground"
        owner = "way_river_stream"
        scale = "world"
        status = "work_order"
        owning_workstream = "River Water Manager"
        required_context = [pscustomobject]@{
            way_family = "way_river_stream"
            ground = "water_shallow"
            shape_mask = "8_neighbor"
        }
        required_masks_or_roles = @($ReferenceMasks)
        target_variants = 4
        qa_labels = @("style_family_fallback")
    }
}

function Add-WayCandidateSet($Source, $Ways, [string]$WorkOrderId, [string]$Kind) {
    $workOrder = $Ways.production_work_orders | Where-Object { (Get-TilesetProperty $_ "id" "") -eq $WorkOrderId } | Select-Object -First 1
    if ($null -eq $workOrder) {
        throw "Missing work order '$WorkOrderId'."
    }
    $owner = Get-TilesetProperty $workOrder "owner" ""
    $context = Get-TilesetProperty $workOrder "required_context" ([pscustomobject]@{})
    $ground = Get-TilesetProperty $context "ground" ""
    $scale = Get-TilesetProperty $workOrder "scale" "world"
    $targetVariants = [int](Get-TilesetProperty $workOrder "target_variants" 4)
    $targetMasks = @()
    foreach ($token in (ConvertTo-TilesetArray (Get-TilesetProperty $workOrder "required_masks_or_roles" @()))) {
        if ([string]$token -match '^mask(?<mask>\d{3})$') {
            $targetMasks += [int]$Matches.mask
        }
    }
    if ($targetMasks.Count -eq 0) {
        throw "Work order '$WorkOrderId' has no maskNNN slots."
    }

    $slug = "$($owner)_on_$($ground)"
    $directory = Join-Path $resolvedSourceRoot "way_segment/$slug"
    New-Item -ItemType Directory -Force -Path $directory | Out-Null

    $generated = @()
    foreach ($mask in $targetMasks) {
        for ($variantIndex = 1; $variantIndex -le $targetVariants; ++$variantIndex) {
            $variant = 100 + $variantIndex
            $maskToken = Format-MaskToken $mask
            $variantToken = Format-VariantToken $variant
            $fileName = "fantasy_neutral__$scale`__way_segment__$slug`__$maskToken`__$variantToken.png"
            $outputPath = Join-Path $directory $fileName
            New-WayCandidatePng $Source $mask $variant $Kind $outputPath $TileSize
            $id = "fantasy_neutral.$scale.way_segment.$slug.$maskToken.$variantToken"
            $asset = [pscustomobject]@{
                id = $id
                style = "fantasy_neutral"
                scale = $scale
                asset_class = "way_segment"
                owner = $owner
                work_order_id = $WorkOrderId
                mask = $mask
                variant = $variant
                source_path = ConvertTo-TilesetRelativePath $outputPath
                dimensions = [pscustomobject]@{ width = $TileSize; height = $TileSize }
                tile_shape = "square"
                transparent = $true
                edge_contract = "material_study_mask_anchor_contract_v101"
                tags = @("runtime_review_candidate", "material_study_20260531", "integrated_way_ground", $ground, "mask_anchor_aligned", "candidate_only")
                context = [pscustomobject]@{
                    way_family = $owner
                    ground = $ground
                    surface_interaction = "material-study repaint intended for in-app review"
                    mask_mapping_status = "v101_anchor_contract"
                }
                status = "candidate"
                validation = [pscustomobject]@{
                    human_review = "needs_in_app_user_feedback"
                    notes = "Replacement candidate generated from material_study_roads_rivers_20260531.png after prior road/structure art was rejected in app review."
                }
            }
            $generated += $asset
        }
    }
    $Ways.assets = @($Ways.assets) + $generated
    return [pscustomobject]@{
        work_order_id = $WorkOrderId
        generated_asset_count = $generated.Count
        masks = @($targetMasks | ForEach-Object { Format-MaskToken $_ })
        variants = @(101..(100 + $targetVariants))
    }
}

function Add-StructureCandidateSet($Source, $Taxonomy, [string]$WorkOrderId) {
    $existingCandidates = @($Taxonomy.assets | Where-Object {
        (Get-TilesetProperty $_ "work_order_id" "") -eq $WorkOrderId -and
        (Get-TilesetProperty $_ "status" "") -eq "needs_repaint" -and
        -not [string]::IsNullOrWhiteSpace((Get-TilesetProperty $_ "rejected_in_app_review" ""))
    })
    if ($existingCandidates.Count -eq 0) {
        $existingCandidates = @($Taxonomy.assets | Where-Object { (Get-TilesetProperty $_ "work_order_id" "") -eq $WorkOrderId })
    }
    $sourceContexts = @($existingCandidates | Where-Object {
        [int](Get-TilesetProperty $_ "variant" 0) -le 2 -and
        -not [string]::IsNullOrWhiteSpace((Get-TilesetProperty $_ "role" ""))
    } | Sort-Object { Get-TilesetProperty $_ "role" "" }, { [int](Get-TilesetProperty $_ "variant" 0) })
    if ($sourceContexts.Count -eq 0) {
        throw "No source contexts found for '$WorkOrderId'."
    }

    $directory = Join-Path $resolvedSourceRoot "structure/guard_tower"
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    $generated = @()
    foreach ($sourceAsset in $sourceContexts) {
        $role = Get-TilesetProperty $sourceAsset "role" ""
        $oldVariant = [int](Get-TilesetProperty $sourceAsset "variant" 1)
        $variant = 100 + $oldVariant
        $variantToken = Format-VariantToken $variant
        $fileName = "fantasy_neutral__local__structure__guard_tower__$role`__$variantToken.png"
        $outputPath = Join-Path $directory $fileName
        $context = Get-TilesetProperty $sourceAsset "context" ([pscustomobject]@{})
        New-StructureCandidatePng $Source $context $role $variant $outputPath $TileSize
        $generated += [pscustomobject]@{
            id = "fantasy_neutral.local.structure.guard_tower.$role.$variantToken"
            style = "fantasy_neutral"
            scale = "local"
            asset_class = "structure"
            owner = "guard_tower"
            work_order_id = $WorkOrderId
            role = $role
            structure_role = $role
            variant = $variant
            source_path = ConvertTo-TilesetRelativePath $outputPath
            dimensions = [pscustomobject]@{ width = $TileSize; height = $TileSize }
            tile_shape = "square"
            transparent = $true
            tags = @("runtime_review_candidate", "material_study_20260531", "structure_context_matrix", "candidate_only")
            context = [pscustomobject]@{
                structure_family = "guard_tower"
                ground = Get-TilesetProperty $context "ground" "grass_open"
                entrance_type = Get-TilesetProperty $context "entrance_type" "none"
                orientation = Get-TilesetProperty $context "orientation" "none"
                footprint = "1x1"
                source_sheet = ConvertTo-TilesetRelativePath $resolvedMaterialSourceImage
                note = "Tower body is fixed; only entrance apron changes by context."
            }
            status = "candidate"
            validation = [pscustomobject]@{
                human_review = "needs_in_app_user_feedback"
                notes = "Replacement structure-context candidate with painted ground and road/bridge/cave/dungeon approach apron."
            }
        }
    }
    $Taxonomy.assets = @($Taxonomy.assets) + $generated
    return [pscustomobject]@{
        work_order_id = $WorkOrderId
        generated_asset_count = $generated.Count
        roles = @($generated | ForEach-Object { $_.role } | Sort-Object -Unique)
    }
}

$waysPath = Join-Path $resolvedSourceRoot "ways.json"
$taxonomyPath = Join-Path $resolvedSourceRoot "taxonomy.json"
$ways = Get-Content -LiteralPath $waysPath -Raw | ConvertFrom-Json
$taxonomy = Get-Content -LiteralPath $taxonomyPath -Raw | ConvertFrom-Json

$replaceWayWorkOrders = @(
    "fn_v2_way_dirt_road_on_grass_open",
    "fn_v2_way_dirt_road_on_dirt",
    "fn_v2_way_stone_road_on_stone_paved",
    "fn_v2_way_trail_narrow_on_grass_open",
    "fn_v2_way_trail_narrow_on_dirt"
)
$structureWorkOrder = "fn_v2_guard_tower_ground_entrance_matrix"

$referenceWorkOrder = $ways.production_work_orders | Where-Object { (Get-TilesetProperty $_ "id" "") -eq "fn_v2_way_dirt_road_on_grass_open" } | Select-Object -First 1
$referenceMasks = @(ConvertTo-TilesetArray (Get-TilesetProperty $referenceWorkOrder "required_masks_or_roles" @()))
Ensure-RiverWorkOrder $ways $referenceMasks
$allWayWorkOrders = $replaceWayWorkOrders + @("fn_v2_way_river_stream_on_water_shallow")

$ways.assets = Remove-MaterialStudyAssets @($ways.assets) $allWayWorkOrders
$taxonomy.assets = Remove-MaterialStudyAssets @($taxonomy.assets) @($structureWorkOrder)
$ways.assets = Demote-ExistingCandidates @($ways.assets) $allWayWorkOrders
$taxonomy.assets = Demote-ExistingCandidates @($taxonomy.assets) @($structureWorkOrder)

$source = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $resolvedMaterialSourceImage).Path)
$summaries = @()
try {
    $summaries += Add-WayCandidateSet $source $ways "fn_v2_way_dirt_road_on_grass_open" "dirt"
    $summaries += Add-WayCandidateSet $source $ways "fn_v2_way_dirt_road_on_dirt" "dirt"
    $summaries += Add-WayCandidateSet $source $ways "fn_v2_way_stone_road_on_stone_paved" "stone"
    $summaries += Add-WayCandidateSet $source $ways "fn_v2_way_trail_narrow_on_grass_open" "trail"
    $summaries += Add-WayCandidateSet $source $ways "fn_v2_way_trail_narrow_on_dirt" "trail"
    $summaries += Add-WayCandidateSet $source $ways "fn_v2_way_river_stream_on_water_shallow" "river"
    $summaries += Add-StructureCandidateSet $source $taxonomy $structureWorkOrder
}
finally {
    $source.Dispose()
}

Write-CleanJsonFile $waysPath $ways
Write-CleanJsonFile $taxonomyPath $taxonomy

$summaryPath = Join-Path $resolvedOutputDirectory "material_study_review_candidates_v101_summary.json"
Write-CleanJsonFile $summaryPath ([pscustomobject]@{
    schema_version = 1
    generated_at_local = (Get-Date).ToString("yyyy-MM-ddTHH:mm:ssK")
    material_source = ConvertTo-TilesetRelativePath $resolvedMaterialSourceImage
    status = "candidate_only_needs_in_app_user_feedback"
    generated_work_orders = $summaries
    notes = @(
        "Supersedes previously rejected road/trail and guard-tower context candidates by demoting them to needs_repaint.",
        "Adds first river-stream-on-water-shallow candidate work order so river way art can preview in the exe.",
        "No assets are marked accepted."
    )
})

Write-Host "Generated material-study review candidates:"
foreach ($summary in $summaries) {
    Write-Host "  $($summary.work_order_id): $($summary.generated_asset_count) asset(s)"
}
Write-Host "Summary: $summaryPath"

param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$TowerSpritePath = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2/ai_source/guard_tower_sprite_v301_alpha.png",
    [string]$OutputDirectory = "",
    [int]$TileSize = 128
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")
Add-Type -AssemblyName System.Drawing

$resolvedSourceRoot = Resolve-TilesetPath $SourceRoot
$resolvedTowerSpritePath = Resolve-TilesetPath $TowerSpritePath
if (-not (Test-Path -LiteralPath $resolvedTowerSpritePath)) {
    throw "Missing tower sprite: $resolvedTowerSpritePath"
}
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $resolvedSourceRoot "review/contextual_candidates"
}
$resolvedOutputDirectory = Resolve-TilesetPath $OutputDirectory
New-Item -ItemType Directory -Force -Path $resolvedOutputDirectory | Out-Null

$reviewTag = "map_art_v301_real_assets_20260531"
$roadWorkOrders = @(
    "fn_v2_way_dirt_road_on_grass_open",
    "fn_v2_way_dirt_road_on_dirt",
    "fn_v2_way_stone_road_on_stone_paved"
)
$trailWorkOrders = @(
    "fn_v2_way_trail_narrow_on_grass_open",
    "fn_v2_way_trail_narrow_on_dirt"
)
$riverWorkOrder = "fn_v2_way_river_stream_on_water_shallow"
$bridgeWorkOrders = @(
    "fn_v2_bridge_wood_simple_shallow_river_sequence",
    "fn_v2_bridge_stone_shallow_river_canal_sequence"
)
$structureWorkOrder = "fn_v2_guard_tower_ground_entrance_matrix"

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

function Get-StableStringSeed([string]$Value) {
    $sum = 0
    foreach ($ch in $Value.ToCharArray()) {
        $sum = (($sum * 31) + [int][char]$ch) -band 0x7fffffff
    }
    return $sum
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

function Draw-Branch($Graphics, [Drawing.Pen]$Pen, [Drawing.PointF]$Center, [string]$Direction, [int]$Seed, [double]$CurveStrength) {
    $exit = Get-ExitPoint $Direction $TileSize
    $vector = Get-DirectionVector $Direction
    $perpX = -[double]$vector.Y
    $perpY = [double]$vector.X
    $jitter = ((($Seed % 19) - 9) / 9.0) * $CurveStrength
    $c1 = [Drawing.PointF]::new(
        [float]($Center.X + ([double]$vector.X * 18.0) + ($perpX * $jitter)),
        [float]($Center.Y + ([double]$vector.Y * 18.0) + ($perpY * $jitter)))
    $c2 = [Drawing.PointF]::new(
        [float]($exit.X - ([double]$vector.X * 28.0) - ($perpX * $jitter * 0.4)),
        [float]($exit.Y - ([double]$vector.Y * 28.0) - ($perpY * $jitter * 0.4)))
    $Graphics.DrawBezier($Pen, $Center, $c1, $c2, $exit)
}

function Draw-ContinuousPair($Graphics, [Drawing.Pen]$Pen, [string]$A, [string]$B, [int]$Seed, [double]$CurveStrength) {
    $start = Get-ExitPoint $A $TileSize
    $end = Get-ExitPoint $B $TileSize
    $vector = Get-DirectionVector $A
    $perpX = -[double]$vector.Y
    $perpY = [double]$vector.X
    $jitter = ((($Seed % 23) - 11) / 11.0) * $CurveStrength
    $c1 = [Drawing.PointF]::new([float](64 + ($perpX * $jitter) - ([double]$vector.X * 20.0)), [float](64 + ($perpY * $jitter) - ([double]$vector.Y * 20.0)))
    $c2 = [Drawing.PointF]::new([float](64 - ($perpX * $jitter) + ([double]$vector.X * 20.0)), [float](64 - ($perpY * $jitter) + ([double]$vector.Y * 20.0)))
    $Graphics.DrawBezier($Pen, $start, $c1, $c2, $end)
}

function Add-VisiblePixelNoise([Drawing.Bitmap]$Bitmap, [int]$Seed, [int]$Strength, [double]$AlphaScale = 1.0) {
    $random = [Random]::new($Seed)
    for ($y = 0; $y -lt $Bitmap.Height; ++$y) {
        for ($x = 0; $x -lt $Bitmap.Width; ++$x) {
            $pixel = $Bitmap.GetPixel($x, $y)
            if ($pixel.A -lt 8) {
                continue
            }
            $jitter = $random.Next(-$Strength, $Strength + 1)
            $r = [Math]::Max(0, [Math]::Min(255, $pixel.R + $jitter))
            $g = [Math]::Max(0, [Math]::Min(255, $pixel.G + [int]($jitter * 0.88)))
            $b = [Math]::Max(0, [Math]::Min(255, $pixel.B + [int]($jitter * 0.72)))
            $a = [Math]::Max(0, [Math]::Min(255, [int]($pixel.A * $AlphaScale)))
            $Bitmap.SetPixel($x, $y, [Drawing.Color]::FromArgb($a, $r, $g, $b))
        }
    }
}

function Draw-IrregularBlob($Graphics, [Drawing.PointF]$Center, [float]$RadiusX, [float]$RadiusY, [Drawing.Color]$Color, [int]$Seed) {
    $random = [Random]::new($Seed)
    $points = @()
    for ($i = 0; $i -lt 16; ++$i) {
        $angle = (2.0 * [Math]::PI * $i) / 16.0
        $scale = 0.78 + ($random.NextDouble() * 0.32)
        $points += [Drawing.PointF]::new(
            [float]($Center.X + ([Math]::Cos($angle) * $RadiusX * $scale)),
            [float]($Center.Y + ([Math]::Sin($angle) * $RadiusY * $scale)))
    }
    $brush = [Drawing.SolidBrush]::new($Color)
    try {
        $Graphics.FillPolygon($brush, $points)
    }
    finally {
        $brush.Dispose()
    }
}

function Draw-WayNetwork($Graphics, [int]$Mask, [int]$Variant, [Drawing.Pen[]]$Pens, [double]$CurveStrength) {
    $directions = @(Get-MaskDirections $Mask)
    $center = [Drawing.PointF]::new([float](64 + (($Variant % 3) - 1) * 0.9), [float](64 + ((($Mask + $Variant) % 3) - 1) * 0.9))
    if ($directions.Count -eq 0) {
        return
    }
    if ($directions.Count -eq 2 -and (Get-OppositeDirection $directions[0]) -eq $directions[1]) {
        foreach ($pen in $Pens) {
            Draw-ContinuousPair $Graphics $pen $directions[0] $directions[1] (($Mask * 101) + $Variant) $CurveStrength
        }
        return
    }
    foreach ($pen in $Pens) {
        foreach ($direction in $directions) {
            Draw-Branch $Graphics $pen $center $direction (($Mask * 103) + ($Variant * 17) + (Get-StableStringSeed $direction)) $CurveStrength
        }
    }
}

function Draw-FlowMarks($Graphics, [int]$Mask, [int]$Variant, [Drawing.Color]$Color) {
    $directions = @(Get-MaskDirections $Mask)
    $pen = [Drawing.Pen]::new($Color, 1.2)
    try {
        $pen.StartCap = [Drawing.Drawing2D.LineCap]::Round
        $pen.EndCap = [Drawing.Drawing2D.LineCap]::Round
        foreach ($direction in $directions) {
            $random = [Random]::new(($Mask * 1009) + ($Variant * 31) + (Get-StableStringSeed $direction))
            $exit = Get-ExitPoint $direction $TileSize
            $vector = Get-DirectionVector $direction
            $perpX = -[double]$vector.Y
            $perpY = [double]$vector.X
            for ($i = 0; $i -lt 3; ++$i) {
                $t = 0.28 + ($random.NextDouble() * 0.54)
                $side = ($random.NextDouble() - 0.5) * 34.0
                $length = 6.0 + ($random.NextDouble() * 8.0)
                $cx = 64 + (($exit.X - 64) * $t) + ($perpX * $side)
                $cy = 64 + (($exit.Y - 64) * $t) + ($perpY * $side)
                $Graphics.DrawLine(
                    $pen,
                    [Drawing.PointF]::new([float]($cx - ([double]$vector.X * $length * 0.5)), [float]($cy - ([double]$vector.Y * $length * 0.5))),
                    [Drawing.PointF]::new([float]($cx + ([double]$vector.X * $length * 0.5)), [float]($cy + ([double]$vector.Y * $length * 0.5))))
            }
        }
    }
    finally {
        $pen.Dispose()
    }
}

function New-RiverPng([int]$Mask, [int]$Variant, [string]$OutputPath) {
    $bitmap = [Drawing.Bitmap]::new($TileSize, $TileSize, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $outerPen = $null
    $bankPen = $null
    $bodyPen = $null
    $deepPen = $null
    try {
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.Clear([Drawing.Color]::Transparent)

        $directions = @(Get-MaskDirections $Mask)
        $outer = [Drawing.Color]::FromArgb(82, 16, 60, 68)
        $bank = [Drawing.Color]::FromArgb(178, 22, 92, 108)
        $body = [Drawing.Color]::FromArgb(235, 35, 139, 164)
        $deep = [Drawing.Color]::FromArgb(150, 18, 83, 122)
        $outerPen = New-RoundPen $outer 76.0
        $bankPen = New-RoundPen $bank 66.0
        $bodyPen = New-RoundPen $body 54.0
        $deepPen = New-RoundPen $deep 25.0
        $pens = @($outerPen, $bankPen, $bodyPen, $deepPen)

        Draw-WayNetwork $graphics $Mask $Variant $pens 7.0

        if ($directions.Count -eq 1) {
            $direction = $directions[0]
            $vector = Get-DirectionVector $direction
            $sourceCenter = [Drawing.PointF]::new([float](64 - ([double]$vector.X * 13.0)), [float](64 - ([double]$vector.Y * 13.0)))
            Draw-IrregularBlob $graphics $sourceCenter 21.0 16.0 $bank (($Mask * 701) + $Variant)
            Draw-IrregularBlob $graphics $sourceCenter 16.0 12.0 $body (($Mask * 709) + $Variant)
            Draw-IrregularBlob $graphics $sourceCenter 8.0 7.0 $deep (($Mask * 719) + $Variant)
        }

        Add-VisiblePixelNoise $bitmap (($Mask * 439) + ($Variant * 997)) 5 0.96
        Draw-FlowMarks $graphics $Mask $Variant ([Drawing.Color]::FromArgb(120, 222, 247, 242))
        $bitmap.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        if ($deepPen) { $deepPen.Dispose() }
        if ($bodyPen) { $bodyPen.Dispose() }
        if ($bankPen) { $bankPen.Dispose() }
        if ($outerPen) { $outerPen.Dispose() }
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function New-RoadPng([int]$Mask, [int]$Variant, [string]$Kind, [string]$OutputPath) {
    $bitmap = [Drawing.Bitmap]::new($TileSize, $TileSize, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $edgePen = $null
    $bodyPen = $null
    $detailPen = $null
    try {
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.Clear([Drawing.Color]::Transparent)

        if ($Kind -eq "trail") {
            $edgePen = New-RoundPen ([Drawing.Color]::FromArgb(96, 73, 52, 30)) 25.0
            $bodyPen = New-RoundPen ([Drawing.Color]::FromArgb(220, 132, 91, 48)) 17.0
            $detailPen = New-RoundPen ([Drawing.Color]::FromArgb(105, 77, 48, 27)) 1.4
            Draw-WayNetwork $graphics $Mask $Variant @($edgePen, $bodyPen) 7.0
            Draw-FlowMarks $graphics $Mask $Variant ([Drawing.Color]::FromArgb(72, 218, 171, 104))
            Add-VisiblePixelNoise $bitmap (($Mask * 829) + $Variant) 7 0.98
        }
        elseif ($Kind -eq "stone") {
            $edgePen = New-RoundPen ([Drawing.Color]::FromArgb(160, 55, 55, 50)) 52.0
            $bodyPen = New-RoundPen ([Drawing.Color]::FromArgb(238, 128, 126, 114)) 42.0
            $detailPen = New-RoundPen ([Drawing.Color]::FromArgb(118, 55, 55, 51)) 2.0
            Draw-WayNetwork $graphics $Mask $Variant @($edgePen, $bodyPen) 4.5
            $directions = @(Get-MaskDirections $Mask)
            foreach ($direction in $directions) {
                for ($i = 0; $i -lt 3; ++$i) {
                    Draw-Branch $graphics $detailPen ([Drawing.PointF]::new(64, 64)) $direction (($Mask * 307) + ($Variant * 13) + $i) 2.0
                }
            }
            Add-VisiblePixelNoise $bitmap (($Mask * 811) + $Variant) 6 0.98
        }
        else {
            $edgePen = New-RoundPen ([Drawing.Color]::FromArgb(120, 86, 58, 31)) 48.0
            $bodyPen = New-RoundPen ([Drawing.Color]::FromArgb(235, 143, 98, 52)) 38.0
            $detailPen = New-RoundPen ([Drawing.Color]::FromArgb(115, 75, 45, 24)) 2.0
            Draw-WayNetwork $graphics $Mask $Variant @($edgePen, $bodyPen) 5.5
            Draw-FlowMarks $graphics $Mask $Variant ([Drawing.Color]::FromArgb(84, 215, 171, 104))
            Add-VisiblePixelNoise $bitmap (($Mask * 821) + $Variant) 7 0.98
        }

        $bitmap.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        if ($detailPen) { $detailPen.Dispose() }
        if ($bodyPen) { $bodyPen.Dispose() }
        if ($edgePen) { $edgePen.Dispose() }
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function Get-GroundPalette([string]$Ground) {
    switch ($Ground) {
        "stone_paved" { return @{ Base = [Drawing.Color]::FromArgb(238, 118, 120, 112); Edge = [Drawing.Color]::FromArgb(170, 62, 66, 61); Detail = [Drawing.Color]::FromArgb(120, 177, 174, 158) } }
        "dirt" { return @{ Base = [Drawing.Color]::FromArgb(238, 126, 87, 45); Edge = [Drawing.Color]::FromArgb(160, 78, 50, 28); Detail = [Drawing.Color]::FromArgb(105, 185, 136, 76) } }
        "cave_floor" { return @{ Base = [Drawing.Color]::FromArgb(238, 88, 83, 72); Edge = [Drawing.Color]::FromArgb(175, 38, 35, 31); Detail = [Drawing.Color]::FromArgb(120, 138, 128, 104) } }
        "dungeon_floor_stone" { return @{ Base = [Drawing.Color]::FromArgb(238, 85, 92, 98); Edge = [Drawing.Color]::FromArgb(175, 42, 47, 52); Detail = [Drawing.Color]::FromArgb(122, 143, 151, 154) } }
        default { return @{ Base = [Drawing.Color]::FromArgb(238, 72, 135, 72); Edge = [Drawing.Color]::FromArgb(150, 38, 86, 42); Detail = [Drawing.Color]::FromArgb(105, 155, 193, 100) } }
    }
}

function Draw-GroundBase($Graphics, [string]$Ground, [int]$Variant) {
    $palette = Get-GroundPalette $Ground
    $points = @(
        [Drawing.Point]::new(64, 16),
        [Drawing.Point]::new(116, 58),
        [Drawing.Point]::new(64, 112),
        [Drawing.Point]::new(12, 58)
    )
    $path = [Drawing.Drawing2D.GraphicsPath]::new()
    $brush = [Drawing.SolidBrush]::new($palette.Base)
    $edgePen = [Drawing.Pen]::new($palette.Edge, 2.0)
    $detailBrush = [Drawing.SolidBrush]::new($palette.Detail)
    try {
        $path.AddPolygon($points)
        $Graphics.FillPath($brush, $path)
        $Graphics.DrawPath($edgePen, $path)
        $state = $Graphics.Save()
        $Graphics.SetClip($path)
        $random = [Random]::new(31000 + ($Variant * 97) + (Get-StableStringSeed $Ground))
        for ($i = 0; $i -lt 70; ++$i) {
            $Graphics.FillEllipse($detailBrush, [Drawing.Rectangle]::new($random.Next(13, 113), $random.Next(18, 108), $random.Next(1, 4), $random.Next(1, 3)))
        }
        $Graphics.Restore($state)
    }
    finally {
        $detailBrush.Dispose()
        $edgePen.Dispose()
        $brush.Dispose()
        $path.Dispose()
    }
}

function Draw-StructureApproach($Graphics, [string]$Entrance, [string]$Orientation, [int]$Variant) {
    if ($Entrance -eq "none" -or $Orientation -eq "none") {
        return
    }
    $mask = switch ($Orientation) {
        "n" { 1 }
        "e" { 4 }
        "s" { 16 }
        "w" { 64 }
        default { 0 }
    }
    if ($mask -eq 0) {
        return
    }
    $kind = if ($Entrance -eq "way_stone_road" -or $Entrance -eq "dungeon_corridor") { "stone" } else { "dirt" }
    if ($Entrance -eq "way_wood_bridge") {
        $edgePen = New-RoundPen ([Drawing.Color]::FromArgb(175, 64, 41, 20)) 38.0
        $bodyPen = New-RoundPen ([Drawing.Color]::FromArgb(236, 139, 87, 38)) 30.0
        try {
            Draw-WayNetwork $Graphics $mask $Variant @($edgePen, $bodyPen) 1.0
        }
        finally {
            $edgePen.Dispose()
            $bodyPen.Dispose()
        }
        return
    }
    $edge = if ($kind -eq "stone") { [Drawing.Color]::FromArgb(170, 58, 58, 53) } else { [Drawing.Color]::FromArgb(150, 82, 52, 28) }
    $body = if ($kind -eq "stone") { [Drawing.Color]::FromArgb(238, 129, 128, 116) } else { [Drawing.Color]::FromArgb(236, 140, 94, 49) }
    $edgePen2 = New-RoundPen $edge 38.0
    $bodyPen2 = New-RoundPen $body 28.0
    try {
        Draw-WayNetwork $Graphics $mask $Variant @($edgePen2, $bodyPen2) 1.0
    }
    finally {
        $edgePen2.Dispose()
        $bodyPen2.Dispose()
    }
}

function Get-AlphaBounds([Drawing.Bitmap]$Bitmap) {
    $minX = $Bitmap.Width
    $minY = $Bitmap.Height
    $maxX = -1
    $maxY = -1
    for ($y = 0; $y -lt $Bitmap.Height; ++$y) {
        for ($x = 0; $x -lt $Bitmap.Width; ++$x) {
            if ($Bitmap.GetPixel($x, $y).A -gt 8) {
                if ($x -lt $minX) { $minX = $x }
                if ($y -lt $minY) { $minY = $y }
                if ($x -gt $maxX) { $maxX = $x }
                if ($y -gt $maxY) { $maxY = $y }
            }
        }
    }
    if ($maxX -lt $minX -or $maxY -lt $minY) {
        return [Drawing.Rectangle]::new(0, 0, $Bitmap.Width, $Bitmap.Height)
    }
    return [Drawing.Rectangle]::new($minX, $minY, ($maxX - $minX + 1), ($maxY - $minY + 1))
}

function New-StructurePng($TowerSprite, [Drawing.Rectangle]$TowerBounds, $Context, [string]$Role, [int]$Variant, [string]$OutputPath) {
    $bitmap = [Drawing.Bitmap]::new($TileSize, $TileSize, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.Clear([Drawing.Color]::Transparent)
        $ground = [string](Get-TilesetProperty $Context "ground" "grass_open")
        $entrance = [string](Get-TilesetProperty $Context "entrance_type" "none")
        $orientation = [string](Get-TilesetProperty $Context "orientation" "none")
        Draw-GroundBase $graphics $ground $Variant
        Draw-StructureApproach $graphics $entrance $orientation $Variant

        $shadowBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(95, 18, 16, 12))
        try {
            $graphics.FillEllipse($shadowBrush, [Drawing.Rectangle]::new(34, 78, 62, 25))
        }
        finally {
            $shadowBrush.Dispose()
        }

        $scale = [Math]::Min(86.0 / [double]$TowerBounds.Width, 112.0 / [double]$TowerBounds.Height)
        $drawWidth = [int]([double]$TowerBounds.Width * $scale)
        $drawHeight = [int]([double]$TowerBounds.Height * $scale)
        $dest = [Drawing.Rectangle]::new([int](64 - ($drawWidth / 2)), [int](8 + (110 - $drawHeight)), $drawWidth, $drawHeight)
        $graphics.DrawImage($TowerSprite, $dest, $TowerBounds, [Drawing.GraphicsUnit]::Pixel)

        Add-VisiblePixelNoise $bitmap (($Variant * 601) + (Get-StableStringSeed $Role)) 2 1.0
        $bitmap.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function Draw-RiverUnderlay($Graphics, [string]$Role) {
    if ($Role -notin @("threshold_a", "mid_01", "mid_02", "threshold_b")) {
        return
    }
    $outerPen = New-RoundPen ([Drawing.Color]::FromArgb(82, 16, 60, 68)) 76.0
    $bankPen = New-RoundPen ([Drawing.Color]::FromArgb(178, 22, 92, 108)) 66.0
    $bodyPen = New-RoundPen ([Drawing.Color]::FromArgb(235, 35, 139, 164)) 54.0
    $deepPen = New-RoundPen ([Drawing.Color]::FromArgb(150, 18, 83, 122)) 25.0
    try {
        foreach ($pen in @($outerPen, $bankPen, $bodyPen, $deepPen)) {
            $Graphics.DrawLine($pen, [Drawing.PointF]::new(64, -5), [Drawing.PointF]::new(64, 133))
        }
    }
    finally {
        $outerPen.Dispose()
        $bankPen.Dispose()
        $bodyPen.Dispose()
        $deepPen.Dispose()
    }
}

function New-BridgePng([string]$Family, [string]$Role, [int]$Variant, [string]$OutputPath) {
    $bitmap = [Drawing.Bitmap]::new($TileSize, $TileSize, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.Clear([Drawing.Color]::Transparent)
        Draw-RiverUnderlay $graphics $Role

        if ($Family -eq "bridge_stone") {
            $edgePen = [Drawing.Pen]::new([Drawing.Color]::FromArgb(238, 60, 60, 56), 7.0)
            $bodyBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(242, 132, 130, 119))
            $mortarPen = [Drawing.Pen]::new([Drawing.Color]::FromArgb(145, 55, 55, 51), 2.0)
            try {
                $left = 36
                $width = 56
                $graphics.FillRectangle($bodyBrush, [Drawing.Rectangle]::new($left, -4, $width, 136))
                $graphics.DrawLine($edgePen, $left - 3, -4, $left - 3, 132)
                $graphics.DrawLine($edgePen, $left + $width + 3, -4, $left + $width + 3, 132)
                for ($y = -8 + (($Variant % 3) * 5); $y -lt 136; $y += 17) {
                    $graphics.DrawLine($mortarPen, $left + 2, $y, $left + $width - 2, $y)
                    for ($x = $left + 8 + (($y / 17) % 2) * 9; $x -lt $left + $width; $x += 18) {
                        $graphics.DrawLine($mortarPen, $x, $y, $x, $y + 17)
                    }
                }
            }
            finally {
                $edgePen.Dispose()
                $bodyBrush.Dispose()
                $mortarPen.Dispose()
            }
        }
        else {
            $shadowBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(120, 35, 22, 14))
            $deckBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(242, 136, 84, 38))
            $linePen = [Drawing.Pen]::new([Drawing.Color]::FromArgb(155, 63, 36, 18), 3.0)
            $railPen = [Drawing.Pen]::new([Drawing.Color]::FromArgb(235, 70, 42, 20), 5.0)
            try {
                $left = 41
                $width = 46
                $graphics.FillRectangle($shadowBrush, [Drawing.Rectangle]::new($left - 8, -4, $width + 16, 136))
                $graphics.FillRectangle($deckBrush, [Drawing.Rectangle]::new($left, -4, $width, 136))
                for ($y = -8 + (($Variant % 3) * 4); $y -lt 136; $y += 15) {
                    $graphics.DrawLine($linePen, $left + 2, $y, $left + $width - 2, $y + (($Variant % 2) * 2))
                }
                $graphics.DrawLine($railPen, $left - 8, -4, $left - 8, 132)
                $graphics.DrawLine($railPen, $left + $width + 8, -4, $left + $width + 8, 132)
            }
            finally {
                $shadowBrush.Dispose()
                $deckBrush.Dispose()
                $linePen.Dispose()
                $railPen.Dispose()
            }
        }
        Add-VisiblePixelNoise $bitmap ((Get-StableStringSeed "$Family$Role") + $Variant) 4 1.0
        $bitmap.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function Remove-TaggedAssets($Assets, [string[]]$WorkOrderIds, [string]$Tag) {
    return @($Assets | Where-Object {
            $workOrder = Get-TilesetProperty $_ "work_order_id" ""
            $tags = @(ConvertTo-TilesetArray (Get-TilesetProperty $_ "tags" @()))
            -not ($workOrder -in $WorkOrderIds -and $Tag -in $tags)
        })
}

function Demote-ActiveCandidates($Assets, [string[]]$WorkOrderIds, [string]$Note) {
    return @($Assets | ForEach-Object {
            $asset = $_
            $workOrder = Get-TilesetProperty $asset "work_order_id" ""
            $status = Get-TilesetProperty $asset "status" ""
            if ($workOrder -in $WorkOrderIds -and $status -eq "candidate") {
                Set-JsonProperty $asset "status" "needs_repaint"
                Set-JsonProperty $asset "rejected_in_app_review" "2026-05-31 replaced by v301 art candidates"
                $validation = Get-TilesetProperty $asset "validation" ([pscustomobject]@{})
                Set-JsonProperty $validation "replacement_note" $Note
                Set-JsonProperty $asset "validation" $validation
            }
            $asset
        })
}

function Get-ActiveMaskSet($Assets, [string]$WorkOrderId, $WorkOrder) {
    $masks = @($Assets | Where-Object {
            (Get-TilesetProperty $_ "work_order_id" "") -eq $WorkOrderId -and
            (Get-TilesetProperty $_ "status" "") -eq "candidate" -and
            $null -ne (Get-TilesetProperty $_ "mask" $null)
        } | ForEach-Object { [int](Get-TilesetProperty $_ "mask" 0) } | Sort-Object -Unique)
    if ($masks.Count -gt 0) {
        return $masks
    }
    return @(ConvertTo-TilesetArray (Get-TilesetProperty $WorkOrder "required_masks_or_roles" @()) | Where-Object { [string]$_ -match '^mask(?<mask>\d{3})$' } | ForEach-Object { [int]$Matches.mask } | Sort-Object -Unique)
}

function Add-WayCandidateSet($Ways, [string]$WorkOrderId, [string]$Kind) {
    $workOrder = $Ways.production_work_orders | Where-Object { (Get-TilesetProperty $_ "id" "") -eq $WorkOrderId } | Select-Object -First 1
    if ($null -eq $workOrder) {
        throw "Missing work order '$WorkOrderId'."
    }
    $owner = Get-TilesetProperty $workOrder "owner" ""
    $context = Get-TilesetProperty $workOrder "required_context" ([pscustomobject]@{})
    $ground = Get-TilesetProperty $context "ground" ""
    $scale = Get-TilesetProperty $workOrder "scale" "world"
    $targetVariants = [int](Get-TilesetProperty $workOrder "target_variants" 4)
    $masks = @(Get-ActiveMaskSet @($Ways.assets) $WorkOrderId $workOrder)
    $slug = "$($owner)_on_$($ground)"
    $directory = Join-Path $resolvedSourceRoot "way_segment/$slug"
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    $generated = @()
    foreach ($mask in $masks) {
        for ($variantIndex = 1; $variantIndex -le $targetVariants; ++$variantIndex) {
            $variant = 300 + $variantIndex
            $maskToken = Format-MaskToken $mask
            $variantToken = Format-VariantToken $variant
            $fileName = "fantasy_neutral__$scale`__way_segment__$slug`__$maskToken`__$variantToken.png"
            $outputPath = Join-Path $directory $fileName
            if ($Kind -eq "river") {
                New-RiverPng $mask $variant $outputPath
            }
            else {
                New-RoadPng $mask $variant $Kind $outputPath
            }
            $generated += [pscustomobject]@{
                id = "fantasy_neutral.$scale.way_segment.$slug.$maskToken.$variantToken"
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
                edge_contract = "map_art_v301_mask_anchor_contract"
                tags = @("runtime_review_candidate", $reviewTag, "integrated_way_ground", $ground, "mask_anchor_aligned", "candidate_only")
                context = [pscustomobject]@{
                    way_family = $owner
                    ground = $ground
                    surface_interaction = "transparent integrated way overlay without tinted backing"
                    mask_mapping_status = "v301_anchor_contract"
                }
                status = "candidate"
                validation = [pscustomobject]@{
                    human_review = "needs_in_app_user_feedback"
                    notes = "v301 replacement: cleaner transparent road/river candidate art for in-app review."
                }
            }
        }
    }
    $Ways.assets = @($Ways.assets) + $generated
    return [pscustomobject]@{ work_order_id = $WorkOrderId; generated_asset_count = $generated.Count; masks = @($masks | ForEach-Object { Format-MaskToken $_ }); variants = @(301..(300 + $targetVariants)) }
}

function Add-BridgeCandidateSet($Ways, [string]$WorkOrderId) {
    $workOrder = $Ways.production_work_orders | Where-Object { (Get-TilesetProperty $_ "id" "") -eq $WorkOrderId } | Select-Object -First 1
    if ($null -eq $workOrder) {
        throw "Missing work order '$WorkOrderId'."
    }
    $owner = Get-TilesetProperty $workOrder "owner" ""
    $scale = Get-TilesetProperty $workOrder "scale" "world"
    $targetVariants = [int](Get-TilesetProperty $workOrder "target_variants" 3)
    $roles = @(ConvertTo-TilesetArray (Get-TilesetProperty $workOrder "required_masks_or_roles" @()))
    $directory = Join-Path $resolvedSourceRoot "bridge_span/$owner"
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    $generated = @()
    foreach ($role in $roles) {
        $sourceContext = $Ways.assets | Where-Object {
            (Get-TilesetProperty $_ "work_order_id" "") -eq $WorkOrderId -and
            (Get-TilesetProperty $_ "role" "") -eq $role
        } | Select-Object -First 1
        for ($variantIndex = 1; $variantIndex -le $targetVariants; ++$variantIndex) {
            $variant = 300 + $variantIndex
            $variantToken = Format-VariantToken $variant
            $fileName = "fantasy_neutral__$scale`__bridge_span__$owner`__$role`__$variantToken.png"
            $outputPath = Join-Path $directory $fileName
            New-BridgePng $owner $role $variant $outputPath
            $generated += [pscustomobject]@{
                id = "fantasy_neutral.$scale.bridge_span.$owner.$role.$variantToken"
                style = "fantasy_neutral"
                scale = $scale
                asset_class = "bridge_span"
                owner = $owner
                work_order_id = $WorkOrderId
                role = $role
                span_position = $role
                variant = $variant
                source_path = ConvertTo-TilesetRelativePath $outputPath
                dimensions = [pscustomobject]@{ width = $TileSize; height = $TileSize }
                tile_shape = "square"
                transparent = $true
                edge_contract = "bridge_sequence_v301_river_underlay"
                tags = @("runtime_review_candidate", $reviewTag, "bridge_sequence_candidate", "bridge_span", $owner, $role, "candidate_only")
                context = if ($null -ne $sourceContext) { Get-TilesetProperty $sourceContext "context" ([pscustomobject]@{}) } else { [pscustomobject]@{ bridge_family = $owner; span_position = $role } }
                status = "candidate"
                validation = [pscustomobject]@{
                    human_review = "needs_in_app_user_feedback"
                    notes = "v301 replacement bridge candidate with river underlay on water spans so crossings visually match adjacent river tiles."
                }
            }
        }
    }
    $Ways.assets = @($Ways.assets) + $generated
    return [pscustomobject]@{ work_order_id = $WorkOrderId; generated_asset_count = $generated.Count; roles = $roles; variants = @(301..(300 + $targetVariants)) }
}

function Add-StructureCandidateSet($Taxonomy, [string]$WorkOrderId, $TowerSprite, [Drawing.Rectangle]$TowerBounds) {
    $sourceContexts = @($Taxonomy.assets | Where-Object {
            (Get-TilesetProperty $_ "work_order_id" "") -eq $WorkOrderId -and
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
        $variant = 300 + $oldVariant
        $variantToken = Format-VariantToken $variant
        $fileName = "fantasy_neutral__local__structure__guard_tower__$role`__$variantToken.png"
        $outputPath = Join-Path $directory $fileName
        $context = Get-TilesetProperty $sourceAsset "context" ([pscustomobject]@{})
        New-StructurePng $TowerSprite $TowerBounds $context $role $variant $outputPath
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
            tags = @("runtime_review_candidate", $reviewTag, "structure_context_matrix", "imagegen_tower_sprite", "fixed_tower_orientation", "candidate_only")
            context = [pscustomobject]@{
                structure_family = "guard_tower"
                ground = Get-TilesetProperty $context "ground" "grass_open"
                entrance_type = Get-TilesetProperty $context "entrance_type" "none"
                orientation = Get-TilesetProperty $context "orientation" "none"
                footprint = "1x1"
                source_sprite = ConvertTo-TilesetRelativePath $resolvedTowerSpritePath
                note = "Tower sprite is fixed orientation; context changes only ground and approach art."
            }
            status = "candidate"
            validation = [pscustomobject]@{
                human_review = "needs_in_app_user_feedback"
                notes = "v301 replacement guard tower candidate composited from a painted sprite with contextual approaches."
            }
        }
    }
    $Taxonomy.assets = @($Taxonomy.assets) + $generated
    return [pscustomobject]@{ work_order_id = $WorkOrderId; generated_asset_count = $generated.Count; variants = @(301, 302); roles = @($generated | ForEach-Object { $_.role } | Sort-Object -Unique) }
}

function Draw-CandidateContactSheet($Assets, [string]$OutputPath, [string]$Title, [int]$Columns) {
    $cellWidth = 170
    $cellHeight = 164
    $headerHeight = 48
    $rows = [int][Math]::Ceiling($Assets.Count / [double]$Columns)
    $width = ($Columns * $cellWidth) + 24
    $height = $headerHeight + ($rows * $cellHeight) + 18
    $bitmap = [Drawing.Bitmap]::new($width, $height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $titleFont = [Drawing.Font]::new([Drawing.FontFamily]::GenericSansSerif, 14.0, [Drawing.FontStyle]::Bold)
    $labelFont = [Drawing.Font]::new([Drawing.FontFamily]::GenericSansSerif, 8.0, [Drawing.FontStyle]::Regular)
    $titleBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(245, 242, 232))
    $labelBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(235, 232, 220))
    $framePen = [Drawing.Pen]::new([Drawing.Color]::FromArgb(95, 90, 80), 1.0)
    $tileBackBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(58, 79, 75))
    try {
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.Clear([Drawing.Color]::FromArgb(32, 31, 29))
        $graphics.DrawString($Title, $titleFont, $titleBrush, [Drawing.PointF]::new(12, 12))
        for ($i = 0; $i -lt $Assets.Count; ++$i) {
            $asset = $Assets[$i]
            $column = $i % $Columns
            $row = [int][Math]::Floor($i / $Columns)
            $x = 12 + ($column * $cellWidth)
            $y = $headerHeight + ($row * $cellHeight)
            $dest = [Drawing.Rectangle]::new($x + 21, $y + 4, 128, 128)
            $graphics.FillRectangle($tileBackBrush, $dest)
            $sourcePath = Resolve-TilesetPath (Get-TilesetProperty $asset "source_path" "")
            $image = [Drawing.Bitmap]::new($sourcePath)
            try {
                $graphics.DrawImage($image, $dest)
            }
            finally {
                $image.Dispose()
            }
            $graphics.DrawRectangle($framePen, $dest)
            $label = Get-TilesetProperty $asset "role" ""
            if ([string]::IsNullOrWhiteSpace($label)) {
                $label = "$(Format-MaskToken ([int](Get-TilesetProperty $asset "mask" 0))) $(Format-VariantToken ([int](Get-TilesetProperty $asset "variant" 0)))"
            }
            else {
                $label = "$label $(Format-VariantToken ([int](Get-TilesetProperty $asset "variant" 0)))"
            }
            $graphics.DrawString($label, $labelFont, $labelBrush, [Drawing.PointF]::new($x + 5, $y + 136))
        }
        $bitmap.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $tileBackBrush.Dispose()
        $framePen.Dispose()
        $labelBrush.Dispose()
        $titleBrush.Dispose()
        $labelFont.Dispose()
        $titleFont.Dispose()
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

$waysPath = Join-Path $resolvedSourceRoot "ways.json"
$taxonomyPath = Join-Path $resolvedSourceRoot "taxonomy.json"
$ways = Get-Content -LiteralPath $waysPath -Raw | ConvertFrom-Json
$taxonomy = Get-Content -LiteralPath $taxonomyPath -Raw | ConvertFrom-Json

$allWayReplacementWorkOrders = $roadWorkOrders + $trailWorkOrders + @($riverWorkOrder) + $bridgeWorkOrders
$ways.assets = Remove-TaggedAssets @($ways.assets) $allWayReplacementWorkOrders $reviewTag
$taxonomy.assets = Remove-TaggedAssets @($taxonomy.assets) @($structureWorkOrder) $reviewTag

$summaries = @()
$towerSprite = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $resolvedTowerSpritePath).Path)
try {
    $towerBounds = Get-AlphaBounds $towerSprite

    $summaries += Add-WayCandidateSet $ways "fn_v2_way_dirt_road_on_grass_open" "dirt"
    $summaries += Add-WayCandidateSet $ways "fn_v2_way_dirt_road_on_dirt" "dirt"
    $summaries += Add-WayCandidateSet $ways "fn_v2_way_stone_road_on_stone_paved" "stone"
    $summaries += Add-WayCandidateSet $ways "fn_v2_way_trail_narrow_on_grass_open" "trail"
    $summaries += Add-WayCandidateSet $ways "fn_v2_way_trail_narrow_on_dirt" "trail"
    $summaries += Add-WayCandidateSet $ways $riverWorkOrder "river"
    $summaries += Add-BridgeCandidateSet $ways "fn_v2_bridge_wood_simple_shallow_river_sequence"
    $summaries += Add-BridgeCandidateSet $ways "fn_v2_bridge_stone_shallow_river_canal_sequence"
    $summaries += Add-StructureCandidateSet $taxonomy $structureWorkOrder $towerSprite $towerBounds
}
finally {
    $towerSprite.Dispose()
}

$ways.assets = Demote-ActiveCandidates @($ways.assets) $allWayReplacementWorkOrders "Superseded by v301 real-art replacement candidates."
$taxonomy.assets = Demote-ActiveCandidates @($taxonomy.assets) @($structureWorkOrder) "Superseded by v301 real-art structure candidates."

foreach ($asset in $ways.assets) {
    $tags = @(ConvertTo-TilesetArray (Get-TilesetProperty $asset "tags" @()))
    if ($reviewTag -in $tags) {
        Set-JsonProperty $asset "status" "candidate"
    }
}
foreach ($asset in $taxonomy.assets) {
    $tags = @(ConvertTo-TilesetArray (Get-TilesetProperty $asset "tags" @()))
    if ($reviewTag -in $tags) {
        Set-JsonProperty $asset "status" "candidate"
    }
}

Write-CleanJsonFile $waysPath $ways
Write-CleanJsonFile $taxonomyPath $taxonomy

$generatedRoadAssets = @($ways.assets | Where-Object {
        $reviewTag -in @(ConvertTo-TilesetArray (Get-TilesetProperty $_ "tags" @())) -and
        (Get-TilesetProperty $_ "asset_class" "") -eq "way_segment" -and
        (Get-TilesetProperty $_ "work_order_id" "") -ne $riverWorkOrder
    } | Sort-Object { Get-TilesetProperty $_ "work_order_id" "" }, { [int](Get-TilesetProperty $_ "mask" 0) }, { [int](Get-TilesetProperty $_ "variant" 0) })
$generatedRiverAssets = @($ways.assets | Where-Object {
        $reviewTag -in @(ConvertTo-TilesetArray (Get-TilesetProperty $_ "tags" @())) -and
        (Get-TilesetProperty $_ "work_order_id" "") -eq $riverWorkOrder
    } | Sort-Object { [int](Get-TilesetProperty $_ "mask" 0) }, { [int](Get-TilesetProperty $_ "variant" 0) })
$generatedBridgeAssets = @($ways.assets | Where-Object {
        $reviewTag -in @(ConvertTo-TilesetArray (Get-TilesetProperty $_ "tags" @())) -and
        (Get-TilesetProperty $_ "asset_class" "") -eq "bridge_span"
    } | Sort-Object { Get-TilesetProperty $_ "owner" "" }, { Get-TilesetProperty $_ "role" "" }, { [int](Get-TilesetProperty $_ "variant" 0) })
$generatedStructureAssets = @($taxonomy.assets | Where-Object {
        $reviewTag -in @(ConvertTo-TilesetArray (Get-TilesetProperty $_ "tags" @()))
    } | Sort-Object { Get-TilesetProperty $_ "role" "" }, { [int](Get-TilesetProperty $_ "variant" 0) })

$roadSheetPath = Join-Path $resolvedOutputDirectory "roads_v301_v304_clean_overlay_contact_sheet.png"
$riverSheetPath = Join-Path $resolvedOutputDirectory "river_stream_v301_v304_no_hubs_contact_sheet.png"
$bridgeSheetPath = Join-Path $resolvedOutputDirectory "bridge_spans_v301_v303_river_underlay_contact_sheet.png"
$structureSheetPath = Join-Path $resolvedOutputDirectory "guard_tower_contexts_v301_v302_real_sprite_review.png"
Draw-CandidateContactSheet (@($generatedRoadAssets | Select-Object -First 96)) $roadSheetPath "Road/trail v301-v304 clean transparent overlay samples" 8
Draw-CandidateContactSheet $generatedRiverAssets $riverSheetPath "River stream v301-v304 no circular hubs" 8
Draw-CandidateContactSheet $generatedBridgeAssets $bridgeSheetPath "Bridge spans v301-v303 with river underlay" 6
Draw-CandidateContactSheet $generatedStructureAssets $structureSheetPath "Guard tower v301-v302 painted sprite context set" 6

$summaryPath = Join-Path $resolvedOutputDirectory "map_art_v301_real_assets_summary.json"
Write-CleanJsonFile $summaryPath ([pscustomobject]@{
        schema_version = 1
        generated_at_local = (Get-Date).ToString("yyyy-MM-ddTHH:mm:ssK")
        status = "candidate_only_needs_in_app_user_feedback"
        tag = $reviewTag
        tower_sprite = ConvertTo-TilesetRelativePath $resolvedTowerSpritePath
        generated_work_orders = $summaries
        review_sheets = @(
            ConvertTo-TilesetRelativePath $roadSheetPath,
            ConvertTo-TilesetRelativePath $riverSheetPath,
            ConvertTo-TilesetRelativePath $bridgeSheetPath,
            ConvertTo-TilesetRelativePath $structureSheetPath
        )
        notes = @(
            "Replaces active road, trail, river, bridge, and guard-tower candidates with v301+ review candidates.",
            "Roads and trails no longer include a baked greenish backing; transparent pixels remain transparent.",
            "Rivers remove circular hubs and use source/sink shapes for one-sided masks rather than hard dead-end stripes.",
            "Bridge water spans include a river underlay so bridge crossings better match adjacent river tiles.",
            "Guard towers are composited from a painted generated sprite while retaining fixed orientation.",
            "No assets are marked accepted."
        )
    })

Write-Host "Generated v301 map-art candidates:"
foreach ($summary in $summaries) {
    Write-Host "  $($summary.work_order_id): $($summary.generated_asset_count) asset(s)"
}
Write-Host "Summary: $summaryPath"

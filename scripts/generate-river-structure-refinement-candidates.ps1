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

$refinementTag = "river_structure_refinement_20260531"
$riverWorkOrderId = "fn_v2_way_river_stream_on_water_shallow"
$structureWorkOrderId = "fn_v2_guard_tower_ground_entrance_matrix"

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

function Draw-RouteBranch(
    $Graphics,
    [Drawing.Pen]$Pen,
    [Drawing.PointF]$Center,
    [string]$Direction,
    [int]$Seed,
    [double]$CurveStrength = 10.0,
    [double]$Inset = 26.0
) {
    $exit = Get-ExitPoint $Direction $TileSize
    $vector = Get-DirectionVector $Direction
    $perpX = -[double]$vector.Y
    $perpY = [double]$vector.X
    $jitter = ((($Seed % 19) - 9) / 9.0) * $CurveStrength
    $c1 = [Drawing.PointF]::new(
        [float]($Center.X + ([double]$vector.X * 18.0) + ($perpX * $jitter)),
        [float]($Center.Y + ([double]$vector.Y * 18.0) + ($perpY * $jitter)))
    $c2 = [Drawing.PointF]::new(
        [float]($exit.X - ([double]$vector.X * $Inset) - ($perpX * $jitter * 0.45)),
        [float]($exit.Y - ([double]$vector.Y * $Inset) - ($perpY * $jitter * 0.45)))
    $Graphics.DrawBezier($Pen, $Center, $c1, $c2, $exit)
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
            $g = [Math]::Max(0, [Math]::Min(255, $pixel.G + [int]($jitter * 0.9)))
            $b = [Math]::Max(0, [Math]::Min(255, $pixel.B + [int]($jitter * 0.75)))
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
            $x = $random.Next(5, $Bitmap.Width - 5)
            $y = $random.Next(5, $Bitmap.Height - 5)
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

function Draw-RiverFoamMarks($Graphics, [string]$Direction, [Drawing.PointF]$Center, [int]$Seed, [int]$Count) {
    $random = [Random]::new($Seed)
    $exit = Get-ExitPoint $Direction $TileSize
    $vector = Get-DirectionVector $Direction
    $perpX = -[double]$vector.Y
    $perpY = [double]$vector.X
    $foamPen = [Drawing.Pen]::new([Drawing.Color]::FromArgb(128, 220, 248, 246), 2.2)
    $glintPen = [Drawing.Pen]::new([Drawing.Color]::FromArgb(82, 248, 255, 252), 1.2)
    try {
        $foamPen.StartCap = [Drawing.Drawing2D.LineCap]::Round
        $foamPen.EndCap = [Drawing.Drawing2D.LineCap]::Round
        $glintPen.StartCap = [Drawing.Drawing2D.LineCap]::Round
        $glintPen.EndCap = [Drawing.Drawing2D.LineCap]::Round
        for ($i = 0; $i -lt $Count; ++$i) {
            $t = 0.22 + ($random.NextDouble() * 0.68)
            $side = ($random.NextDouble() - 0.5) * 34.0
            $length = 7.0 + ($random.NextDouble() * 15.0)
            $cx = $Center.X + (($exit.X - $Center.X) * $t) + ($perpX * $side)
            $cy = $Center.Y + (($exit.Y - $Center.Y) * $t) + ($perpY * $side)
            $sx = $cx - ([double]$vector.X * $length * 0.5) + ($perpX * 1.5)
            $sy = $cy - ([double]$vector.Y * $length * 0.5) + ($perpY * 1.5)
            $ex = $cx + ([double]$vector.X * $length * 0.5) - ($perpX * 1.5)
            $ey = $cy + ([double]$vector.Y * $length * 0.5) - ($perpY * 1.5)
            $pen = if (($i % 3) -eq 0) { $glintPen } else { $foamPen }
            $Graphics.DrawLine($pen, [Drawing.PointF]::new([float]$sx, [float]$sy), [Drawing.PointF]::new([float]$ex, [float]$ey))
        }
    }
    finally {
        $foamPen.Dispose()
        $glintPen.Dispose()
    }
}

function New-RiverCandidatePng([int]$Mask, [int]$Variant, [string]$OutputPath, [int]$Size) {
    $bitmap = [Drawing.Bitmap]::new($Size, $Size, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $outerPen = $null
    $bankPen = $null
    $bodyPen = $null
    $deepPen = $null
    $outerBrush = $null
    $bankBrush = $null
    $bodyBrush = $null
    try {
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.Clear([Drawing.Color]::Transparent)

        $directions = @(Get-MaskDirections $Mask)
        $centerJitterX = (($Variant % 3) - 1) * 1.6
        $centerJitterY = ((($Variant + $Mask) % 3) - 1) * 1.4
        $center = [Drawing.PointF]::new([float](($Size / 2.0) + $centerJitterX), [float](($Size / 2.0) + $centerJitterY))
        $flowWidth = 72.0 + (($Variant % 2) * 4.0)

        $outerColor = [Drawing.Color]::FromArgb(112, 15, 58, 68)
        $bankColor = [Drawing.Color]::FromArgb(178, 24, 92, 108)
        $bodyColor = [Drawing.Color]::FromArgb(232, 35, 142, 166)
        $deepColor = [Drawing.Color]::FromArgb(168, 24, 91, 126)

        $outerBrush = [Drawing.SolidBrush]::new($outerColor)
        $bankBrush = [Drawing.SolidBrush]::new($bankColor)
        $bodyBrush = [Drawing.SolidBrush]::new($bodyColor)
        $outerPen = New-RoundPen $outerColor ([float]($flowWidth + 19.0))
        $bankPen = New-RoundPen $bankColor ([float]($flowWidth + 11.0))
        $bodyPen = New-RoundPen $bodyColor ([float]$flowWidth)
        $deepPen = New-RoundPen $deepColor ([float]($flowWidth * 0.46))

        foreach ($direction in $directions) {
            Draw-RouteBranch $graphics $outerPen $center $direction (($Mask * 101) + ($Variant * 17) + $direction.Length) 8.5 29.0
        }
        foreach ($direction in $directions) {
            Draw-RouteBranch $graphics $bankPen $center $direction (($Mask * 103) + ($Variant * 19) + $direction.Length) 8.0 28.0
        }
        foreach ($direction in $directions) {
            Draw-RouteBranch $graphics $bodyPen $center $direction (($Mask * 107) + ($Variant * 23) + $direction.Length) 7.2 27.0
        }

        $hubScale = if ($directions.Count -ge 5) { 1.32 } elseif ($directions.Count -ge 3) { 1.08 } else { 0.78 }
        $outerHub = [int](($flowWidth + 20.0) * $hubScale)
        $bankHub = [int](($flowWidth + 11.0) * $hubScale)
        $bodyHub = [int]($flowWidth * $hubScale)
        $graphics.FillEllipse($outerBrush, [Drawing.Rectangle]::new([int]($center.X - ($outerHub / 2.0)), [int]($center.Y - ($outerHub / 2.0)), $outerHub, $outerHub))
        $graphics.FillEllipse($bankBrush, [Drawing.Rectangle]::new([int]($center.X - ($bankHub / 2.0)), [int]($center.Y - ($bankHub / 2.0)), $bankHub, $bankHub))
        $graphics.FillEllipse($bodyBrush, [Drawing.Rectangle]::new([int]($center.X - ($bodyHub / 2.0)), [int]($center.Y - ($bodyHub / 2.0)), $bodyHub, $bodyHub))

        foreach ($direction in $directions) {
            Draw-RouteBranch $graphics $deepPen $center $direction (($Mask * 109) + ($Variant * 29) + $direction.Length) 6.8 27.0
        }

        Add-VisiblePixelNoise $bitmap (($Mask * 439) + ($Variant * 997)) 7 0.96
        Add-RandomMarks $graphics $bitmap (($Mask * 443) + $Variant) ([Drawing.Color]::FromArgb(70, 10, 71, 92)) 42 5
        Add-RandomMarks $graphics $bitmap (($Mask * 449) + $Variant) ([Drawing.Color]::FromArgb(90, 143, 220, 224)) 30 4

        foreach ($direction in $directions) {
            Draw-RiverFoamMarks $graphics $direction $center (($Mask * 113) + ($Variant * 31) + $direction.Length) 5
        }

        $bitmap.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        if ($deepPen) { $deepPen.Dispose() }
        if ($bodyPen) { $bodyPen.Dispose() }
        if ($bankPen) { $bankPen.Dispose() }
        if ($outerPen) { $outerPen.Dispose() }
        if ($bodyBrush) { $bodyBrush.Dispose() }
        if ($bankBrush) { $bankBrush.Dispose() }
        if ($outerBrush) { $outerBrush.Dispose() }
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function Get-GroundPalette([string]$Ground) {
    switch ($Ground) {
        "stone_paved" {
            return @{
                Base = [Drawing.Color]::FromArgb(236, 123, 123, 112)
                Light = [Drawing.Color]::FromArgb(140, 176, 174, 158)
                Dark = [Drawing.Color]::FromArgb(118, 70, 70, 64)
                Edge = [Drawing.Color]::FromArgb(170, 68, 72, 64)
            }
        }
        "dirt" {
            return @{
                Base = [Drawing.Color]::FromArgb(238, 130, 91, 48)
                Light = [Drawing.Color]::FromArgb(110, 188, 141, 78)
                Dark = [Drawing.Color]::FromArgb(120, 70, 45, 25)
                Edge = [Drawing.Color]::FromArgb(150, 80, 53, 31)
            }
        }
        "cave_floor" {
            return @{
                Base = [Drawing.Color]::FromArgb(238, 93, 88, 76)
                Light = [Drawing.Color]::FromArgb(112, 137, 128, 104)
                Dark = [Drawing.Color]::FromArgb(138, 39, 35, 31)
                Edge = [Drawing.Color]::FromArgb(170, 43, 40, 35)
            }
        }
        "dungeon_floor_stone" {
            return @{
                Base = [Drawing.Color]::FromArgb(238, 91, 96, 100)
                Light = [Drawing.Color]::FromArgb(132, 144, 151, 154)
                Dark = [Drawing.Color]::FromArgb(140, 45, 49, 53)
                Edge = [Drawing.Color]::FromArgb(170, 45, 51, 56)
            }
        }
        default {
            return @{
                Base = [Drawing.Color]::FromArgb(238, 78, 139, 75)
                Light = [Drawing.Color]::FromArgb(100, 153, 190, 102)
                Dark = [Drawing.Color]::FromArgb(110, 38, 92, 47)
                Edge = [Drawing.Color]::FromArgb(150, 42, 88, 43)
            }
        }
    }
}

function Draw-TexturedGround($Graphics, [string]$Ground, [int]$Variant) {
    $palette = Get-GroundPalette $Ground
    $points = @(
        [Drawing.Point]::new(64, 14),
        [Drawing.Point]::new(118, 58),
        [Drawing.Point]::new(64, 112),
        [Drawing.Point]::new(10, 58)
    )
    $path = [Drawing.Drawing2D.GraphicsPath]::new()
    $baseBrush = [Drawing.SolidBrush]::new($palette.Base)
    $edgePen = [Drawing.Pen]::new($palette.Edge, 3.0)
    $lightBrush = [Drawing.SolidBrush]::new($palette.Light)
    $darkBrush = [Drawing.SolidBrush]::new($palette.Dark)
    try {
        $path.AddPolygon($points)
        $Graphics.FillPath($baseBrush, $path)
        $Graphics.DrawPath($edgePen, $path)

        $state = $Graphics.Save()
        $Graphics.SetClip($path)
        $random = [Random]::new(24000 + ($Variant * 101) + $Ground.Length)
        for ($i = 0; $i -lt 120; ++$i) {
            $brush = if (($i % 3) -eq 0) { $lightBrush } else { $darkBrush }
            $x = $random.Next(11, 116)
            $y = $random.Next(17, 109)
            $w = $random.Next(1, 5)
            $h = $random.Next(1, 4)
            $Graphics.FillEllipse($brush, [Drawing.Rectangle]::new($x, $y, $w, $h))
        }
        if ($Ground -in @("stone_paved", "dungeon_floor_stone")) {
            $linePen = [Drawing.Pen]::new($palette.Dark, 1.0)
            try {
                for ($offset = -64; $offset -le 96; $offset += 22) {
                    $Graphics.DrawLine($linePen, [Drawing.Point]::new($offset, 58), [Drawing.Point]::new($offset + 88, 14))
                    $Graphics.DrawLine($linePen, [Drawing.Point]::new($offset, 58), [Drawing.Point]::new($offset + 88, 112))
                }
            }
            finally {
                $linePen.Dispose()
            }
        }
        $Graphics.Restore($state)
    }
    finally {
        $path.Dispose()
        $baseBrush.Dispose()
        $edgePen.Dispose()
        $lightBrush.Dispose()
        $darkBrush.Dispose()
    }
}

function Get-ApproachPoints([string]$Orientation) {
    switch ($Orientation) {
        "n" { return @{ Start = [Drawing.PointF]::new(64, -8); End = [Drawing.PointF]::new(64, 62) } }
        "e" { return @{ Start = [Drawing.PointF]::new(136, 64); End = [Drawing.PointF]::new(82, 74) } }
        "s" { return @{ Start = [Drawing.PointF]::new(64, 136); End = [Drawing.PointF]::new(64, 89) } }
        "w" { return @{ Start = [Drawing.PointF]::new(-8, 64); End = [Drawing.PointF]::new(46, 74) } }
    }
    return $null
}

function Draw-ApproachCurve($Graphics, [Drawing.Pen]$Pen, [string]$Orientation, [double]$Wobble) {
    $points = Get-ApproachPoints $Orientation
    if ($null -eq $points) {
        return
    }
    $start = $points.Start
    $end = $points.End
    $midX = (($start.X + $end.X) / 2.0)
    $midY = (($start.Y + $end.Y) / 2.0)
    $c1 = [Drawing.PointF]::new([float]$midX, [float]($midY - $Wobble))
    $c2 = [Drawing.PointF]::new([float]$midX, [float]($midY + $Wobble))
    if ($Orientation -in @("e", "w")) {
        $c1 = [Drawing.PointF]::new([float]($midX - $Wobble), [float]$midY)
        $c2 = [Drawing.PointF]::new([float]($midX + $Wobble), [float]$midY)
    }
    $Graphics.DrawBezier($Pen, $start, $c1, $c2, $end)
}

function Draw-StructureApproach($Graphics, [string]$Entrance, [string]$Orientation, [int]$Variant) {
    if ($Entrance -eq "none" -or $Orientation -eq "none") {
        return
    }

    $edgeColor = [Drawing.Color]::FromArgb(152, 60, 46, 29)
    $bodyColor = [Drawing.Color]::FromArgb(232, 137, 95, 48)
    $detailColor = [Drawing.Color]::FromArgb(118, 67, 42, 22)
    $width = 33.0
    if ($Entrance -eq "way_stone_road" -or $Entrance -eq "dungeon_corridor") {
        $edgeColor = [Drawing.Color]::FromArgb(164, 50, 53, 52)
        $bodyColor = [Drawing.Color]::FromArgb(234, 132, 132, 122)
        $detailColor = [Drawing.Color]::FromArgb(126, 68, 68, 63)
        $width = 38.0
    }
    elseif ($Entrance -eq "way_wood_bridge") {
        $edgeColor = [Drawing.Color]::FromArgb(174, 72, 49, 25)
        $bodyColor = [Drawing.Color]::FromArgb(238, 147, 93, 43)
        $detailColor = [Drawing.Color]::FromArgb(150, 60, 37, 18)
        $width = 36.0
    }
    elseif ($Entrance -eq "cave_tunnel") {
        $edgeColor = [Drawing.Color]::FromArgb(186, 30, 28, 25)
        $bodyColor = [Drawing.Color]::FromArgb(230, 83, 75, 61)
        $detailColor = [Drawing.Color]::FromArgb(162, 22, 20, 18)
        $width = 40.0
    }

    $edgePen = New-RoundPen $edgeColor ([float]($width + 10.0))
    $bodyPen = New-RoundPen $bodyColor ([float]$width)
    $detailPen = New-RoundPen $detailColor 2.0
    try {
        Draw-ApproachCurve $Graphics $edgePen $Orientation (($Variant % 3) * 2.0)
        Draw-ApproachCurve $Graphics $bodyPen $Orientation (($Variant % 3) * 2.0)

        if ($Entrance -eq "way_wood_bridge") {
            $points = Get-ApproachPoints $Orientation
            $railPen = New-RoundPen ([Drawing.Color]::FromArgb(190, 68, 41, 18)) 3.0
            $plankPen = [Drawing.Pen]::new([Drawing.Color]::FromArgb(150, 84, 50, 22), 2.0)
            try {
                if ($Orientation -in @("n", "s")) {
                    for ($y = [Math]::Min($points.Start.Y, $points.End.Y); $y -le [Math]::Max($points.Start.Y, $points.End.Y); $y += 13) {
                        $Graphics.DrawLine($plankPen, [Drawing.PointF]::new(45, [float]$y), [Drawing.PointF]::new(83, [float]($y + (($Variant % 2) * 2))))
                    }
                    $Graphics.DrawLine($railPen, [Drawing.Point]::new(43, [int]$points.Start.Y), [Drawing.Point]::new(50, [int]$points.End.Y))
                    $Graphics.DrawLine($railPen, [Drawing.Point]::new(85, [int]$points.Start.Y), [Drawing.Point]::new(78, [int]$points.End.Y))
                }
                else {
                    for ($x = [Math]::Min($points.Start.X, $points.End.X); $x -le [Math]::Max($points.Start.X, $points.End.X); $x += 13) {
                        $Graphics.DrawLine($plankPen, [Drawing.PointF]::new([float]$x, 48), [Drawing.PointF]::new([float]($x + (($Variant % 2) * 2)), 85))
                    }
                    $Graphics.DrawLine($railPen, [Drawing.Point]::new([int]$points.Start.X, 45), [Drawing.Point]::new([int]$points.End.X, 55))
                    $Graphics.DrawLine($railPen, [Drawing.Point]::new([int]$points.Start.X, 86), [Drawing.Point]::new([int]$points.End.X, 82))
                }
            }
            finally {
                $railPen.Dispose()
                $plankPen.Dispose()
            }
        }
        else {
            Draw-ApproachCurve $Graphics $detailPen $Orientation (($Variant % 3) * 2.0)
        }
    }
    finally {
        $edgePen.Dispose()
        $bodyPen.Dispose()
        $detailPen.Dispose()
    }
}

function Draw-GuardTower($Graphics, [int]$Variant) {
    $shadowBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(105, 26, 22, 18))
    $leftWallBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(242, 116, 116, 106))
    $rightWallBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(242, 88, 91, 86))
    $frontWallBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(242, 135, 132, 119))
    $topBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(244, 171, 168, 148))
    $roofBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(240, 129, 74, 40))
    $roofShadeBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(210, 82, 45, 28))
    $darkBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(232, 42, 37, 31))
    $lightBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(128, 211, 204, 178))
    $outlinePen = [Drawing.Pen]::new([Drawing.Color]::FromArgb(220, 42, 38, 32), 2.0)
    $mortarPen = [Drawing.Pen]::new([Drawing.Color]::FromArgb(94, 59, 57, 51), 1.0)
    try {
        $Graphics.FillEllipse($shadowBrush, [Drawing.Rectangle]::new(33, 79, 66, 24))

        $leftWall = @(
            [Drawing.Point]::new(39, 47),
            [Drawing.Point]::new(64, 58),
            [Drawing.Point]::new(64, 96),
            [Drawing.Point]::new(39, 84)
        )
        $rightWall = @(
            [Drawing.Point]::new(64, 58),
            [Drawing.Point]::new(89, 47),
            [Drawing.Point]::new(91, 84),
            [Drawing.Point]::new(64, 96)
        )
        $frontWall = @(
            [Drawing.Point]::new(39, 84),
            [Drawing.Point]::new(64, 96),
            [Drawing.Point]::new(91, 84),
            [Drawing.Point]::new(91, 92),
            [Drawing.Point]::new(64, 106),
            [Drawing.Point]::new(38, 92)
        )
        $top = @(
            [Drawing.Point]::new(39, 46),
            [Drawing.Point]::new(64, 33),
            [Drawing.Point]::new(90, 46),
            [Drawing.Point]::new(64, 61)
        )
        $roof = @(
            [Drawing.Point]::new(64, 17),
            [Drawing.Point]::new(96, 42),
            [Drawing.Point]::new(64, 59),
            [Drawing.Point]::new(32, 42)
        )
        $roofShade = @(
            [Drawing.Point]::new(64, 17),
            [Drawing.Point]::new(96, 42),
            [Drawing.Point]::new(64, 59)
        )

        $Graphics.FillPolygon($leftWallBrush, $leftWall)
        $Graphics.FillPolygon($rightWallBrush, $rightWall)
        $Graphics.FillPolygon($frontWallBrush, $frontWall)
        $Graphics.FillPolygon($topBrush, $top)
        $Graphics.FillPolygon($roofBrush, $roof)
        $Graphics.FillPolygon($roofShadeBrush, $roofShade)

        $Graphics.DrawPolygon($outlinePen, $leftWall)
        $Graphics.DrawPolygon($outlinePen, $rightWall)
        $Graphics.DrawPolygon($outlinePen, $frontWall)
        $Graphics.DrawPolygon($outlinePen, $roof)

        for ($y = 60; $y -le 88; $y += 9) {
            $Graphics.DrawLine($mortarPen, [Drawing.Point]::new(43, $y), [Drawing.Point]::new(63, $y + 8))
            $Graphics.DrawLine($mortarPen, [Drawing.Point]::new(66, $y + 8), [Drawing.Point]::new(87, $y))
        }
        $Graphics.FillRectangle($darkBrush, [Drawing.Rectangle]::new(48, 68, 7, 10))
        $Graphics.FillRectangle($darkBrush, [Drawing.Rectangle]::new(75, 67, 7, 10))
        $Graphics.FillPolygon($darkBrush, @(
                [Drawing.Point]::new(58, 88),
                [Drawing.Point]::new(64, 84),
                [Drawing.Point]::new(71, 88),
                [Drawing.Point]::new(71, 100),
                [Drawing.Point]::new(64, 104),
                [Drawing.Point]::new(58, 100)
            ))
        $Graphics.FillRectangle($lightBrush, [Drawing.Rectangle]::new(47, 49, 12, 4))
        $Graphics.FillRectangle($lightBrush, [Drawing.Rectangle]::new(68, 39, 11, 4))
        if ($Variant % 2 -eq 0) {
            $flagPen = [Drawing.Pen]::new([Drawing.Color]::FromArgb(220, 62, 46, 28), 2.0)
            $flagBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(230, 91, 126, 80))
            try {
                $Graphics.DrawLine($flagPen, [Drawing.Point]::new(64, 17), [Drawing.Point]::new(64, 6))
                $Graphics.FillPolygon($flagBrush, @(
                        [Drawing.Point]::new(65, 6),
                        [Drawing.Point]::new(82, 11),
                        [Drawing.Point]::new(65, 17)
                    ))
            }
            finally {
                $flagPen.Dispose()
                $flagBrush.Dispose()
            }
        }
    }
    finally {
        $shadowBrush.Dispose()
        $leftWallBrush.Dispose()
        $rightWallBrush.Dispose()
        $frontWallBrush.Dispose()
        $topBrush.Dispose()
        $roofBrush.Dispose()
        $roofShadeBrush.Dispose()
        $darkBrush.Dispose()
        $lightBrush.Dispose()
        $outlinePen.Dispose()
        $mortarPen.Dispose()
    }
}

function New-StructureCandidatePng($Context, [string]$Role, [int]$Variant, [string]$OutputPath, [int]$Size) {
    $bitmap = [Drawing.Bitmap]::new($Size, $Size, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.Clear([Drawing.Color]::Transparent)

        $ground = [string](Get-TilesetProperty $Context "ground" "grass_open")
        $entrance = [string](Get-TilesetProperty $Context "entrance_type" "none")
        $orientation = [string](Get-TilesetProperty $Context "orientation" "none")

        Draw-TexturedGround $graphics $ground $Variant
        Draw-StructureApproach $graphics $entrance $orientation $Variant

        if ($entrance -eq "cave_tunnel") {
            $mouthBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(210, 25, 23, 21))
            $rockPen = [Drawing.Pen]::new([Drawing.Color]::FromArgb(190, 96, 88, 70), 3.0)
            try {
                $points = Get-ApproachPoints $orientation
                if ($null -ne $points) {
                    $Graphics.FillEllipse($mouthBrush, [Drawing.Rectangle]::new([int]($points.End.X - 18), [int]($points.End.Y - 12), 36, 24))
                    $Graphics.DrawEllipse($rockPen, [Drawing.Rectangle]::new([int]($points.End.X - 20), [int]($points.End.Y - 14), 40, 28))
                }
            }
            finally {
                $mouthBrush.Dispose()
                $rockPen.Dispose()
            }
        }

        Draw-GuardTower $graphics $Variant
        Add-VisiblePixelNoise $bitmap (($Variant * 601) + $Role.Length) 3 1.0
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
                Set-JsonProperty $asset "rejected_in_app_review" "2026-05-31 replaced by river/structure refinement candidates"
                $validation = Get-TilesetProperty $asset "validation" ([pscustomobject]@{})
                Set-JsonProperty $validation "replacement_note" $Note
                Set-JsonProperty $asset "validation" $validation
            }
            $asset
        })
}

function Add-RiverCandidateSet($Ways, [string]$WorkOrderId) {
    $workOrder = $Ways.production_work_orders | Where-Object { (Get-TilesetProperty $_ "id" "") -eq $WorkOrderId } | Select-Object -First 1
    if ($null -eq $workOrder) {
        throw "Missing work order '$WorkOrderId'."
    }
    $owner = Get-TilesetProperty $workOrder "owner" "way_river_stream"
    $context = Get-TilesetProperty $workOrder "required_context" ([pscustomobject]@{})
    $ground = Get-TilesetProperty $context "ground" "water_shallow"
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
            $variant = 200 + $variantIndex
            $maskToken = Format-MaskToken $mask
            $variantToken = Format-VariantToken $variant
            $fileName = "fantasy_neutral__$scale`__way_segment__$slug`__$maskToken`__$variantToken.png"
            $outputPath = Join-Path $directory $fileName
            New-RiverCandidatePng $mask $variant $outputPath $TileSize
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
                edge_contract = "river_stream_mask_anchor_contract_v201"
                tags = @("runtime_review_candidate", $refinementTag, "integrated_way_ground", $ground, "mask_anchor_aligned", "candidate_only")
                context = [pscustomobject]@{
                    way_family = $owner
                    ground = $ground
                    surface_interaction = "wider painted river channel with diagonal-aware mask branches"
                    mask_mapping_status = "v201_anchor_contract"
                }
                status = "candidate"
                validation = [pscustomobject]@{
                    human_review = "needs_in_app_user_feedback"
                    notes = "Replacement river candidate with broader channel, darker banks, inner current, and branch detail for diagonal masks."
                }
            }
        }
    }
    $Ways.assets = @($Ways.assets) + $generated
    return [pscustomobject]@{
        work_order_id = $WorkOrderId
        generated_asset_count = $generated.Count
        masks = @($targetMasks | ForEach-Object { Format-MaskToken $_ })
        variants = @(201..(200 + $targetVariants))
    }
}

function Add-StructureCandidateSet($Taxonomy, [string]$WorkOrderId) {
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
        $variant = 200 + $oldVariant
        $variantToken = Format-VariantToken $variant
        $fileName = "fantasy_neutral__local__structure__guard_tower__$role`__$variantToken.png"
        $outputPath = Join-Path $directory $fileName
        $context = Get-TilesetProperty $sourceAsset "context" ([pscustomobject]@{})
        New-StructureCandidatePng $context $role $variant $outputPath $TileSize
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
            tags = @("runtime_review_candidate", $refinementTag, "structure_context_matrix", "fixed_tower_orientation", "candidate_only")
            context = [pscustomobject]@{
                structure_family = "guard_tower"
                ground = Get-TilesetProperty $context "ground" "grass_open"
                entrance_type = Get-TilesetProperty $context "entrance_type" "none"
                orientation = Get-TilesetProperty $context "orientation" "none"
                footprint = "1x1"
                note = "Tower body is fixed; ground base and approach apron change by context."
            }
            status = "candidate"
            validation = [pscustomobject]@{
                human_review = "needs_in_app_user_feedback"
                notes = "Replacement guard-tower structure-context candidate with contextual ground and entrance art."
            }
        }
    }
    $Taxonomy.assets = @($Taxonomy.assets) + $generated
    return [pscustomobject]@{
        work_order_id = $WorkOrderId
        generated_asset_count = $generated.Count
        roles = @($generated | ForEach-Object { $_.role } | Sort-Object -Unique)
        variants = @(201, 202)
    }
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
        $framePen.Dispose()
        $tileBackBrush.Dispose()
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

$ways.assets = Remove-TaggedAssets @($ways.assets) @($riverWorkOrderId) $refinementTag
$taxonomy.assets = Remove-TaggedAssets @($taxonomy.assets) @($structureWorkOrderId) $refinementTag
$ways.assets = Demote-ActiveCandidates @($ways.assets) @($riverWorkOrderId) "Superseded by v201+ river refinement candidates."
$taxonomy.assets = Demote-ActiveCandidates @($taxonomy.assets) @($structureWorkOrderId) "Superseded by v201+ guard-tower structure refinement candidates."

$summaries = @()
$summaries += Add-RiverCandidateSet $ways $riverWorkOrderId
$summaries += Add-StructureCandidateSet $taxonomy $structureWorkOrderId

Write-CleanJsonFile $waysPath $ways
Write-CleanJsonFile $taxonomyPath $taxonomy

$generatedRiverAssets = @($ways.assets | Where-Object {
        (Get-TilesetProperty $_ "work_order_id" "") -eq $riverWorkOrderId -and
        $refinementTag -in @(ConvertTo-TilesetArray (Get-TilesetProperty $_ "tags" @()))
    } | Sort-Object { [int](Get-TilesetProperty $_ "mask" 0) }, { [int](Get-TilesetProperty $_ "variant" 0) })
$generatedStructureAssets = @($taxonomy.assets | Where-Object {
        (Get-TilesetProperty $_ "work_order_id" "") -eq $structureWorkOrderId -and
        $refinementTag -in @(ConvertTo-TilesetArray (Get-TilesetProperty $_ "tags" @()))
    } | Sort-Object { Get-TilesetProperty $_ "role" "" }, { [int](Get-TilesetProperty $_ "variant" 0) })

$riverContactPath = Join-Path $resolvedOutputDirectory "river_stream_v201_v204_contact_sheet.png"
$structureContactPath = Join-Path $resolvedOutputDirectory "guard_tower_contexts_v201_v202_matrix_review.png"
Draw-CandidateContactSheet $generatedRiverAssets $riverContactPath "River stream v201-v204 candidate set" 8
Draw-CandidateContactSheet $generatedStructureAssets $structureContactPath "Guard tower context v201-v202 candidate set" 6

$summaryPath = Join-Path $resolvedOutputDirectory "river_structure_refinement_v201_summary.json"
Write-CleanJsonFile $summaryPath ([pscustomobject]@{
        schema_version = 1
        generated_at_local = (Get-Date).ToString("yyyy-MM-ddTHH:mm:ssK")
        status = "candidate_only_needs_in_app_user_feedback"
        tag = $refinementTag
        generated_work_orders = $summaries
        review_sheets = @(
            ConvertTo-TilesetRelativePath $riverContactPath,
            ConvertTo-TilesetRelativePath $structureContactPath
        )
        notes = @(
            "Old active river and guard-tower candidates were demoted to needs_repaint, not deleted.",
            "New river art is broader and diagonal-aware, with visible banks/current detail over water_shallow.",
            "New guard-tower art keeps a fixed tower orientation and varies only the ground and approach context.",
            "No assets are marked accepted."
        )
    })

Write-Host "Generated river/structure refinement candidates:"
foreach ($summary in $summaries) {
    Write-Host "  $($summary.work_order_id): $($summary.generated_asset_count) asset(s)"
}
Write-Host "River sheet: $riverContactPath"
Write-Host "Structure sheet: $structureContactPath"
Write-Host "Summary: $summaryPath"

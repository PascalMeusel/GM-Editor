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

$reviewTag = "map_art_v401_cohesive_full_tile_20260531"
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
$structureWorkOrders = @(
    "fn_v2_guard_tower_ground_entrance_matrix",
    "fn_v2_settlement_house_ground_entrance_matrix",
    "fn_v2_village_cluster_ground_entrance_matrix"
)

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

function New-Color([int]$R, [int]$G, [int]$B, [int]$A = 255) {
    return [Drawing.Color]::FromArgb($A, $R, $G, $B)
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
    $jitter = ((($Seed % 23) - 11) / 11.0) * $CurveStrength
    $c1 = [Drawing.PointF]::new(
        [float]($Center.X + ([double]$vector.X * 20.0) + ($perpX * $jitter)),
        [float]($Center.Y + ([double]$vector.Y * 20.0) + ($perpY * $jitter)))
    $c2 = [Drawing.PointF]::new(
        [float]($exit.X - ([double]$vector.X * 30.0) - ($perpX * $jitter * 0.35)),
        [float]($exit.Y - ([double]$vector.Y * 30.0) - ($perpY * $jitter * 0.35)))
    $Graphics.DrawBezier($Pen, $Center, $c1, $c2, $exit)
}

function Draw-ContinuousPair($Graphics, [Drawing.Pen]$Pen, [string]$A, [string]$B, [int]$Seed, [double]$CurveStrength) {
    $start = Get-ExitPoint $A $TileSize
    $end = Get-ExitPoint $B $TileSize
    $center = $TileSize / 2.0
    $vector = Get-DirectionVector $A
    $perpX = -[double]$vector.Y
    $perpY = [double]$vector.X
    $jitter = ((($Seed % 29) - 14) / 14.0) * $CurveStrength
    $c1 = [Drawing.PointF]::new([float]($center + ($perpX * $jitter) - ([double]$vector.X * 22.0)), [float]($center + ($perpY * $jitter) - ([double]$vector.Y * 22.0)))
    $c2 = [Drawing.PointF]::new([float]($center - ($perpX * $jitter) + ([double]$vector.X * 22.0)), [float]($center - ($perpY * $jitter) + ([double]$vector.Y * 22.0)))
    $Graphics.DrawBezier($Pen, $start, $c1, $c2, $end)
}

function Draw-WayNetwork($Graphics, [int]$Mask, [int]$Variant, [Drawing.Pen[]]$Pens, [double]$CurveStrength) {
    $directions = @(Get-MaskDirections $Mask)
    if ($directions.Count -eq 0) {
        return
    }
    $centerValue = $TileSize / 2.0
    $center = [Drawing.PointF]::new([float]($centerValue + (($Variant % 3) - 1) * 0.8), [float]($centerValue + ((($Mask + $Variant) % 3) - 1) * 0.8))
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

function Get-GroundPalette([string]$Ground) {
    switch ($Ground) {
        "stone_paved" { return @{ Base = New-Color 132 132 122; Light = New-Color 158 155 140 120; Dark = New-Color 75 78 72 90; Accent = New-Color 184 176 146 85 } }
        "dirt" { return @{ Base = New-Color 143 103 61; Light = New-Color 176 134 76 100; Dark = New-Color 85 58 33 75; Accent = New-Color 112 79 46 85 } }
        "cave_floor" { return @{ Base = New-Color 88 80 67; Light = New-Color 125 112 88 95; Dark = New-Color 45 39 33 80; Accent = New-Color 148 132 94 70 } }
        "dungeon_floor_stone" { return @{ Base = New-Color 92 96 98; Light = New-Color 135 139 136 95; Dark = New-Color 45 49 52 80; Accent = New-Color 160 150 128 65 } }
        "water_shallow" { return @{ Base = New-Color 47 139 164; Light = New-Color 102 190 196 90; Dark = New-Color 25 82 122 90; Accent = New-Color 206 235 220 70 } }
        default { return @{ Base = New-Color 91 137 67; Light = New-Color 128 166 83 80; Dark = New-Color 55 91 48 70; Accent = New-Color 196 178 86 70 } }
    }
}

function Draw-TerrainBase($Graphics, [string]$Ground, [int]$Seed, [switch]$ForRiver) {
    $palette = Get-GroundPalette $Ground
    $Graphics.Clear($palette.Base)
    $random = [Random]::new((Get-RandomSeed (9000 + [int64]$Seed + [int64](Get-StableStringSeed $Ground))))
    $brush = [Drawing.SolidBrush]::new($palette.Light)
    try {
        $count = if ($ForRiver) { 115 } else { 150 }
        for ($i = 0; $i -lt $count; ++$i) {
            $brush.Color = if (($i % 5) -eq 0) { $palette.Accent } elseif (($i % 2) -eq 0) { $palette.Dark } else { $palette.Light }
            $w = $random.Next(2, 8)
            $h = $random.Next(1, 5)
            $Graphics.FillEllipse($brush, [Drawing.Rectangle]::new($random.Next(-2, $TileSize), $random.Next(-2, $TileSize), $w, $h))
        }
    }
    finally {
        $brush.Dispose()
    }
}

function Draw-FlowMarks($Graphics, [int]$Mask, [int]$Variant, [Drawing.Color]$Color, [float]$Width) {
    $directions = @(Get-MaskDirections $Mask)
    $pen = New-RoundPen $Color $Width
    try {
        foreach ($direction in $directions) {
            $random = [Random]::new((Get-RandomSeed (($Mask * 1009) + ($Variant * 31) + [int64](Get-StableStringSeed $direction))))
            $exit = Get-ExitPoint $direction $TileSize
            $vector = Get-DirectionVector $direction
            $perpX = -[double]$vector.Y
            $perpY = [double]$vector.X
            for ($i = 0; $i -lt 2; ++$i) {
                $t = 0.30 + ($random.NextDouble() * 0.48)
                $side = ($random.NextDouble() - 0.5) * 24.0
                $length = 6.0 + ($random.NextDouble() * 9.0)
                $cx = ($TileSize / 2.0) + (($exit.X - ($TileSize / 2.0)) * $t) + ($perpX * $side)
                $cy = ($TileSize / 2.0) + (($exit.Y - ($TileSize / 2.0)) * $t) + ($perpY * $side)
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
    $bankPen = $null
    $shadowPen = $null
    $bodyPen = $null
    $deepPen = $null
    try {
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        Draw-TerrainBase $graphics "water_shallow" (($Mask * 11) + $Variant) -ForRiver

        $bankPen = New-RoundPen (New-Color 46 151 174 82) 62.0
        $shadowPen = New-RoundPen (New-Color 35 124 158 105) 50.0
        $bodyPen = New-RoundPen (New-Color 72 171 190 120) 38.0
        $deepPen = New-RoundPen (New-Color 23 94 128 62) 15.0
        Draw-WayNetwork $graphics $Mask $Variant @($bankPen, $shadowPen, $bodyPen, $deepPen) 5.0
        Draw-FlowMarks $graphics $Mask $Variant (New-Color 208 239 228 95) 1.7
        $bitmap.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        if ($deepPen) { $deepPen.Dispose() }
        if ($bodyPen) { $bodyPen.Dispose() }
        if ($shadowPen) { $shadowPen.Dispose() }
        if ($bankPen) { $bankPen.Dispose() }
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function Draw-StonePathDetail($Graphics, [int]$Mask, [int]$Variant) {
    $directions = @(Get-MaskDirections $Mask)
    $pen = New-RoundPen (New-Color 76 76 70 105) 1.5
    try {
        foreach ($direction in $directions) {
            for ($i = 0; $i -lt 3; ++$i) {
                Draw-Branch $Graphics $pen ([Drawing.PointF]::new($TileSize / 2.0, $TileSize / 2.0)) $direction (($Mask * 307) + ($Variant * 13) + $i) 1.4
            }
        }
    }
    finally {
        $pen.Dispose()
    }
}

function New-RoadPng([int]$Mask, [int]$Variant, [string]$Kind, [string]$Ground, [string]$OutputPath) {
    $bitmap = [Drawing.Bitmap]::new($TileSize, $TileSize, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $edgePen = $null
    $bodyPen = $null
    $rutPen = $null
    try {
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $visualGround = if ($Kind -eq "stone") { "grass_open" } else { $Ground }
        Draw-TerrainBase $graphics $visualGround (($Mask * 17) + $Variant)

        if ($Kind -eq "trail") {
            $edgePen = New-RoundPen (New-Color 74 55 32 135) 25.0
            $bodyPen = New-RoundPen (New-Color 139 95 52 235) 16.0
            $rutPen = New-RoundPen (New-Color 80 54 32 95) 1.5
            Draw-WayNetwork $graphics $Mask $Variant @($edgePen, $bodyPen) 6.0
            Draw-FlowMarks $graphics $Mask $Variant (New-Color 225 184 115 65) 1.2
        }
        elseif ($Kind -eq "stone") {
            $edgePen = New-RoundPen (New-Color 72 74 68 170) 43.0
            $bodyPen = New-RoundPen (New-Color 146 144 132 242) 33.0
            Draw-WayNetwork $graphics $Mask $Variant @($edgePen, $bodyPen) 3.2
            Draw-StonePathDetail $graphics $Mask $Variant
        }
        else {
            $edgePen = New-RoundPen (New-Color 79 55 32 170) 47.0
            $bodyPen = New-RoundPen (New-Color 155 106 58 244) 35.0
            $rutPen = New-RoundPen (New-Color 88 57 31 105) 2.0
            Draw-WayNetwork $graphics $Mask $Variant @($edgePen, $bodyPen) 4.5
            Draw-WayNetwork $graphics $Mask ($Variant + 19) @($rutPen) 2.0
        }

        $bitmap.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        if ($rutPen) { $rutPen.Dispose() }
        if ($bodyPen) { $bodyPen.Dispose() }
        if ($edgePen) { $edgePen.Dispose() }
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function Get-StructureRoleContext([string]$Role, [int]$Variant, [string]$Owner) {
    switch ($Role) {
        "entrance_n" { return [pscustomobject]@{ ground = "grass_open"; entrance_type = "way_dirt_road"; orientation = "n" } }
        "entrance_e" { return [pscustomobject]@{ ground = "grass_open"; entrance_type = "way_dirt_road"; orientation = "e" } }
        "entrance_s" { return [pscustomobject]@{ ground = "grass_open"; entrance_type = "way_dirt_road"; orientation = "s" } }
        "entrance_w" { return [pscustomobject]@{ ground = "grass_open"; entrance_type = "way_dirt_road"; orientation = "w" } }
        "entrance_dirt_n" { return [pscustomobject]@{ ground = "dirt"; entrance_type = "way_dirt_road"; orientation = "n" } }
        "entrance_dirt_w" { return [pscustomobject]@{ ground = "dirt"; entrance_type = "way_dirt_road"; orientation = "w" } }
        "entrance_stone_e" { return [pscustomobject]@{ ground = "stone_paved"; entrance_type = "way_stone_road"; orientation = "e" } }
        "entrance_stone_s" { return [pscustomobject]@{ ground = "stone_paved"; entrance_type = "way_stone_road"; orientation = "s" } }
        "bridgehead" {
            if (($Variant % 2) -eq 0) { return [pscustomobject]@{ ground = "stone_paved"; entrance_type = "way_wood_bridge"; orientation = "s" } }
            return [pscustomobject]@{ ground = "grass_open"; entrance_type = "way_wood_bridge"; orientation = "s" }
        }
        "cave_threshold" {
            if (($Variant % 2) -eq 0) { return [pscustomobject]@{ ground = "cave_floor"; entrance_type = "cave_tunnel"; orientation = "n" } }
            return [pscustomobject]@{ ground = "cave_floor"; entrance_type = "cave_tunnel"; orientation = "s" }
        }
        "dungeon_threshold" {
            if (($Variant % 2) -eq 0) { return [pscustomobject]@{ ground = "dungeon_floor_stone"; entrance_type = "dungeon_corridor"; orientation = "w" } }
            return [pscustomobject]@{ ground = "dungeon_floor_stone"; entrance_type = "dungeon_corridor"; orientation = "e" }
        }
        default {
            if (($Variant % 2) -eq 0) { return [pscustomobject]@{ ground = "dirt"; entrance_type = "none"; orientation = "none" } }
            return [pscustomobject]@{ ground = "grass_open"; entrance_type = "none"; orientation = "none" }
        }
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
    if ($Entrance -eq "way_wood_bridge") {
        $edgePen = New-RoundPen (New-Color 64 41 20 210) 38.0
        $bodyPen = New-RoundPen (New-Color 139 87 38 245) 29.0
        try { Draw-WayNetwork $Graphics $mask $Variant @($edgePen, $bodyPen) 1.0 }
        finally { $edgePen.Dispose(); $bodyPen.Dispose() }
        return
    }
    $stone = $Entrance -eq "way_stone_road" -or $Entrance -eq "dungeon_corridor"
    $edge = if ($stone) { New-Color 66 68 62 190 } else { New-Color 79 55 32 175 }
    $body = if ($stone) { New-Color 145 143 130 242 } else { New-Color 155 106 58 244 }
    $edgePen2 = New-RoundPen $edge 37.0
    $bodyPen2 = New-RoundPen $body 27.0
    try { Draw-WayNetwork $Graphics $mask $Variant @($edgePen2, $bodyPen2) 1.0 }
    finally { $edgePen2.Dispose(); $bodyPen2.Dispose() }
}

function Fill-Polygon($Graphics, [Drawing.Color]$Color, $Points) {
    $brush = [Drawing.SolidBrush]::new($Color)
    try { $Graphics.FillPolygon($brush, $Points) }
    finally { $brush.Dispose() }
}

function Fill-Rect($Graphics, [Drawing.Color]$Color, [int]$X, [int]$Y, [int]$W, [int]$H) {
    $brush = [Drawing.SolidBrush]::new($Color)
    try { $Graphics.FillRectangle($brush, [Drawing.Rectangle]::new($X, $Y, $W, $H)) }
    finally { $brush.Dispose() }
}

function Fill-Ellipse($Graphics, [Drawing.Color]$Color, [int]$X, [int]$Y, [int]$W, [int]$H) {
    $brush = [Drawing.SolidBrush]::new($Color)
    try { $Graphics.FillEllipse($brush, [Drawing.Rectangle]::new($X, $Y, $W, $H)) }
    finally { $brush.Dispose() }
}

function Draw-House($Graphics, [int]$X, [int]$Y, [double]$Scale, [Drawing.Color]$Roof, [Drawing.Color]$Wall) {
    $w = [int](31 * $Scale)
    $h = [int](23 * $Scale)
    Fill-Ellipse $Graphics (New-Color 20 17 12 70) ($X - 4) ($Y + $h - 3) ($w + 8) 12
    Fill-Rect $Graphics $Wall $X ($Y + [int](10 * $Scale)) $w $h
    $points = @(
        [Drawing.Point]::new($X - [int](3 * $Scale), $Y + [int](12 * $Scale)),
        [Drawing.Point]::new($X + [int]($w / 2), $Y - [int](2 * $Scale)),
        [Drawing.Point]::new($X + $w + [int](3 * $Scale), $Y + [int](12 * $Scale))
    )
    Fill-Polygon $Graphics $Roof $points
    Fill-Rect $Graphics (New-Color 71 45 29 230) ($X + [int](12 * $Scale)) ($Y + [int](23 * $Scale)) ([int](7 * $Scale)) ([int](10 * $Scale))
    Fill-Rect $Graphics (New-Color 218 185 110 210) ($X + [int](4 * $Scale)) ($Y + [int](18 * $Scale)) ([int](6 * $Scale)) ([int](5 * $Scale))
    Fill-Rect $Graphics (New-Color 218 185 110 210) ($X + [int](22 * $Scale)) ($Y + [int](18 * $Scale)) ([int](6 * $Scale)) ([int](5 * $Scale))
}

function Draw-StructureShape($Graphics, [string]$Owner, [int]$Variant) {
    if ($Owner -eq "village_cluster") {
        Draw-House $Graphics 28 49 0.82 (New-Color 122 54 39) (New-Color 191 151 91)
        Draw-House $Graphics 63 43 0.92 (New-Color 101 60 42) (New-Color 181 137 86)
        Draw-House $Graphics 47 71 0.74 (New-Color 145 76 45) (New-Color 199 162 100)
        Fill-Ellipse $Graphics (New-Color 83 103 56 210) 34 92 17 9
        return
    }

    if ($Owner -eq "settlement_house") {
        Draw-House $Graphics 42 50 1.25 (New-Color 134 57 40) (New-Color 199 158 96)
        Fill-Rect $Graphics (New-Color 88 62 37 190) 34 89 60 5
        return
    }

    Fill-Ellipse $Graphics (New-Color 18 16 12 85) 34 82 63 20
    Fill-Rect $Graphics (New-Color 92 61 36 245) 47 40 34 55
    Fill-Rect $Graphics (New-Color 62 42 28 245) 42 62 6 34
    Fill-Rect $Graphics (New-Color 62 42 28 245) 80 62 6 34
    Fill-Rect $Graphics (New-Color 135 91 48 245) 38 33 52 16
    $roof = @(
        [Drawing.Point]::new(34, 35),
        [Drawing.Point]::new(64, 16),
        [Drawing.Point]::new(94, 35)
    )
    Fill-Polygon $Graphics (New-Color 111 49 37 250) $roof
    Fill-Rect $Graphics (New-Color 231 190 102 220) 58 51 12 10
    Fill-Rect $Graphics (New-Color 60 42 28 230) 57 77 14 18
}

function New-StructurePng([string]$Owner, [string]$Role, [int]$Variant, $Context, [string]$OutputPath) {
    $bitmap = [Drawing.Bitmap]::new($TileSize, $TileSize, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $ground = [string](Get-TilesetProperty $Context "ground" "grass_open")
        $entrance = [string](Get-TilesetProperty $Context "entrance_type" "none")
        $orientation = [string](Get-TilesetProperty $Context "orientation" "none")
        Draw-TerrainBase $graphics $ground (($Variant * 37) + (Get-StableStringSeed "$Owner$Role"))
        Draw-StructureApproach $graphics $entrance $orientation $Variant
        Draw-StructureShape $graphics $Owner $Variant
        $bitmap.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function Draw-RiverUnderlay($Graphics, [string]$Role) {
    if ($Role -notin @("threshold_a", "mid_01", "mid_02", "threshold_b")) {
        Draw-TerrainBase $Graphics "grass_open" (Get-StableStringSeed $Role) -ForRiver
        return
    }
    Draw-TerrainBase $Graphics "water_shallow" (Get-StableStringSeed $Role) -ForRiver
    $bankPen = New-RoundPen (New-Color 46 151 174 82) 64.0
    $shadowPen = New-RoundPen (New-Color 35 124 158 105) 51.0
    $bodyPen = New-RoundPen (New-Color 72 171 190 120) 39.0
    $deepPen = New-RoundPen (New-Color 23 94 128 62) 16.0
    try {
        foreach ($pen in @($bankPen, $shadowPen, $bodyPen, $deepPen)) {
            $Graphics.DrawLine($pen, [Drawing.PointF]::new($TileSize / 2.0, -5), [Drawing.PointF]::new($TileSize / 2.0, $TileSize + 5))
        }
    }
    finally {
        $bankPen.Dispose()
        $shadowPen.Dispose()
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
        Draw-RiverUnderlay $graphics $Role

        if ($Family -eq "bridge_stone") {
            Fill-Rect $graphics (New-Color 65 65 60 235) 34 -4 60 136
            Fill-Rect $graphics (New-Color 143 141 128 248) 40 -4 48 136
            $mortarPen = [Drawing.Pen]::new((New-Color 72 72 66 135), 2.0)
            try {
                for ($y = -8 + (($Variant % 3) * 5); $y -lt 136; $y += 17) {
                    $graphics.DrawLine($mortarPen, 42, $y, 86, $y)
                    for ($x = 48 + (($y / 17) % 2) * 10; $x -lt 86; $x += 19) {
                        $graphics.DrawLine($mortarPen, $x, $y, $x, $y + 17)
                    }
                }
            }
            finally {
                $mortarPen.Dispose()
            }
        }
        else {
            Fill-Rect $graphics (New-Color 45 28 16 110) 32 -4 64 136
            Fill-Rect $graphics (New-Color 139 86 42 248) 41 -4 46 136
            $linePen = [Drawing.Pen]::new((New-Color 72 43 24 145), 3.0)
            $railPen = [Drawing.Pen]::new((New-Color 72 43 24 235), 5.0)
            try {
                for ($y = -8 + (($Variant % 3) * 4); $y -lt 136; $y += 15) {
                    $graphics.DrawLine($linePen, 43, $y, 85, $y + (($Variant % 2) * 2))
                }
                $graphics.DrawLine($railPen, 33, -4, 33, 132)
                $graphics.DrawLine($railPen, 95, -4, 95, 132)
            }
            finally {
                $linePen.Dispose()
                $railPen.Dispose()
            }
        }
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
                Set-JsonProperty $asset "rejected_in_app_review" "2026-05-31 replaced by v401 cohesive candidates"
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

function Ensure-StructureWorkOrder($Taxonomy, [string]$Owner, [string]$WorkOrderId, [string]$QaLabel) {
    $existing = $Taxonomy.production_work_orders | Where-Object { (Get-TilesetProperty $_ "id" "") -eq $WorkOrderId } | Select-Object -First 1
    $roles = @(
        "ground_only",
        "entrance_n",
        "entrance_e",
        "entrance_s",
        "entrance_w",
        "entrance_dirt_n",
        "entrance_dirt_w",
        "entrance_stone_e",
        "entrance_stone_s",
        "bridgehead",
        "cave_threshold",
        "dungeon_threshold"
    )
    $workOrder = [pscustomobject]@{
        id = $WorkOrderId
        asset_class = "structure_context_matrix"
        owner = $Owner
        scale = "local"
        status = "work_order"
        owning_workstream = "Engine Integration Manager"
        required_context = [pscustomobject]@{
            structure_family = $Owner
            grounds = @("grass_open", "dirt", "stone_paved", "cave_floor", "dungeon_floor_stone")
            entrance_types = @("none", "way_dirt_road", "way_stone_road", "way_wood_bridge", "cave_tunnel", "dungeon_corridor")
            orientations = @("n", "e", "s", "w")
        }
        required_masks_or_roles = $roles
        target_variants = 2
        qa_labels = @($QaLabel, "style_family_fallback")
    }
    if ($null -eq $existing) {
        $Taxonomy.production_work_orders = @($Taxonomy.production_work_orders) + @($workOrder)
    }
    else {
        foreach ($property in $workOrder.PSObject.Properties) {
            Set-JsonProperty $existing $property.Name $property.Value
        }
    }
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
            $variant = 400 + $variantIndex
            $maskToken = Format-MaskToken $mask
            $variantToken = Format-VariantToken $variant
            $fileName = "fantasy_neutral__$scale`__way_segment__$slug`__$maskToken`__$variantToken.png"
            $outputPath = Join-Path $directory $fileName
            if ($Kind -eq "river") {
                New-RiverPng $mask $variant $outputPath
            }
            else {
                New-RoadPng $mask $variant $Kind $ground $outputPath
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
                transparent = $false
                edge_contract = "map_art_v401_full_tile_mask_anchor_contract"
                tags = @("runtime_review_candidate", $reviewTag, "integrated_way_ground", $ground, "mask_anchor_aligned", "candidate_only")
                context = [pscustomobject]@{
                    way_family = $owner
                    ground = $ground
                    surface_interaction = "opaque full-tile candidate with matched ground and integrated way"
                    mask_mapping_status = "v401_anchor_contract"
                }
                status = "candidate"
                validation = [pscustomobject]@{
                    human_review = "needs_in_app_user_feedback"
                    notes = "v401 full-tile replacement: cohesive ground, road, trail, and river candidates for in-app review."
                }
            }
        }
    }
    $Ways.assets = @($Ways.assets) + $generated
    return [pscustomobject]@{ work_order_id = $WorkOrderId; generated_asset_count = $generated.Count; masks = @($masks | ForEach-Object { Format-MaskToken $_ }); variants = @(401..(400 + $targetVariants)) }
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
            $variant = 400 + $variantIndex
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
                transparent = $false
                edge_contract = "bridge_sequence_v401_matching_river_underlay"
                tags = @("runtime_review_candidate", $reviewTag, "bridge_sequence_candidate", "bridge_span", $owner, $role, "candidate_only")
                context = if ($null -ne $sourceContext) { Get-TilesetProperty $sourceContext "context" ([pscustomobject]@{}) } else { [pscustomobject]@{ bridge_family = $owner; span_position = $role } }
                status = "candidate"
                validation = [pscustomobject]@{
                    human_review = "needs_in_app_user_feedback"
                    notes = "v401 bridge candidate with the same river underlay palette as river-stream tiles."
                }
            }
        }
    }
    $Ways.assets = @($Ways.assets) + $generated
    return [pscustomobject]@{ work_order_id = $WorkOrderId; generated_asset_count = $generated.Count; roles = $roles; variants = @(401..(400 + $targetVariants)) }
}

function Add-StructureCandidateSet($Taxonomy, [string]$WorkOrderId, [string]$Owner) {
    $workOrder = $Taxonomy.production_work_orders | Where-Object { (Get-TilesetProperty $_ "id" "") -eq $WorkOrderId } | Select-Object -First 1
    if ($null -eq $workOrder) {
        throw "Missing work order '$WorkOrderId'."
    }
    $roles = @(ConvertTo-TilesetArray (Get-TilesetProperty $workOrder "required_masks_or_roles" @()))
    $directory = Join-Path $resolvedSourceRoot "structure/$Owner"
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    $generated = @()
    foreach ($role in $roles) {
        for ($variantIndex = 1; $variantIndex -le 2; ++$variantIndex) {
            $variant = 400 + $variantIndex
            $variantToken = Format-VariantToken $variant
            $fileName = "fantasy_neutral__local__structure__$Owner`__$role`__$variantToken.png"
            $outputPath = Join-Path $directory $fileName
            $context = Get-StructureRoleContext $role $variant $Owner
            New-StructurePng $Owner $role $variant $context $outputPath
            $generated += [pscustomobject]@{
                id = "fantasy_neutral.local.structure.$Owner.$role.$variantToken"
                style = "fantasy_neutral"
                scale = "local"
                asset_class = "structure"
                owner = $Owner
                work_order_id = $WorkOrderId
                role = $role
                structure_role = $role
                variant = $variant
                source_path = ConvertTo-TilesetRelativePath $outputPath
                dimensions = [pscustomobject]@{ width = $TileSize; height = $TileSize }
                tile_shape = "square"
                transparent = $false
                tags = @("runtime_review_candidate", $reviewTag, "structure_context_matrix", "fixed_structure_orientation", $Owner, "candidate_only")
                context = [pscustomobject]@{
                    structure_family = $Owner
                    ground = Get-TilesetProperty $context "ground" "grass_open"
                    entrance_type = Get-TilesetProperty $context "entrance_type" "none"
                    orientation = Get-TilesetProperty $context "orientation" "none"
                    footprint = "1x1"
                    note = "Structure sprite is not rotated; context changes only ground and approach art."
                }
                status = "candidate"
                validation = [pscustomobject]@{
                    human_review = "needs_in_app_user_feedback"
                    notes = "v401 cohesive structure candidate with contextual ground and approaches."
                }
            }
        }
    }
    $Taxonomy.assets = @($Taxonomy.assets) + $generated
    return [pscustomobject]@{ work_order_id = $WorkOrderId; generated_asset_count = $generated.Count; variants = @(401, 402); roles = @($generated | ForEach-Object { $_.role } | Sort-Object -Unique) }
}

function Draw-CandidateContactSheet($Assets, [string]$OutputPath, [string]$Title, [int]$Columns) {
    if ($Assets.Count -eq 0) {
        return
    }
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
    $titleBrush = [Drawing.SolidBrush]::new((New-Color 245 242 232))
    $labelBrush = [Drawing.SolidBrush]::new((New-Color 235 232 220))
    $framePen = [Drawing.Pen]::new((New-Color 95 90 80), 1.0)
    try {
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.Clear((New-Color 32 31 29))
        $graphics.DrawString($Title, $titleFont, $titleBrush, [Drawing.PointF]::new(12, 12))
        for ($i = 0; $i -lt $Assets.Count; ++$i) {
            $asset = $Assets[$i]
            $column = $i % $Columns
            $row = [int][Math]::Floor($i / $Columns)
            $x = 12 + ($column * $cellWidth)
            $y = $headerHeight + ($row * $cellHeight)
            $dest = [Drawing.Rectangle]::new($x + 21, $y + 4, 128, 128)
            $sourcePath = Resolve-TilesetPath (Get-TilesetProperty $asset "source_path" "")
            $image = [Drawing.Bitmap]::new($sourcePath)
            try { $graphics.DrawImage($image, $dest) }
            finally { $image.Dispose() }
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

Ensure-StructureWorkOrder $taxonomy "guard_tower" "fn_v2_guard_tower_ground_entrance_matrix" "tower_cave_grass_halo"
Ensure-StructureWorkOrder $taxonomy "settlement_house" "fn_v2_settlement_house_ground_entrance_matrix" "structure_road_threshold"
Ensure-StructureWorkOrder $taxonomy "village_cluster" "fn_v2_village_cluster_ground_entrance_matrix" "structure_road_threshold"

$allWayReplacementWorkOrders = $roadWorkOrders + $trailWorkOrders + @($riverWorkOrder) + $bridgeWorkOrders
$ways.assets = Remove-TaggedAssets @($ways.assets) $allWayReplacementWorkOrders $reviewTag
$taxonomy.assets = Remove-TaggedAssets @($taxonomy.assets) $structureWorkOrders $reviewTag

$ways.assets = Demote-ActiveCandidates @($ways.assets) $allWayReplacementWorkOrders "Superseded by v401 full-tile cohesive map-art candidates."
$taxonomy.assets = Demote-ActiveCandidates @($taxonomy.assets) $structureWorkOrders "Superseded by v401 structure-family candidates."

$summaries = @()
$summaries += Add-WayCandidateSet $ways "fn_v2_way_dirt_road_on_grass_open" "dirt"
$summaries += Add-WayCandidateSet $ways "fn_v2_way_dirt_road_on_dirt" "dirt"
$summaries += Add-WayCandidateSet $ways "fn_v2_way_stone_road_on_stone_paved" "stone"
$summaries += Add-WayCandidateSet $ways "fn_v2_way_trail_narrow_on_grass_open" "trail"
$summaries += Add-WayCandidateSet $ways "fn_v2_way_trail_narrow_on_dirt" "trail"
$summaries += Add-WayCandidateSet $ways $riverWorkOrder "river"
$summaries += Add-BridgeCandidateSet $ways "fn_v2_bridge_wood_simple_shallow_river_sequence"
$summaries += Add-BridgeCandidateSet $ways "fn_v2_bridge_stone_shallow_river_canal_sequence"
$summaries += Add-StructureCandidateSet $taxonomy "fn_v2_guard_tower_ground_entrance_matrix" "guard_tower"
$summaries += Add-StructureCandidateSet $taxonomy "fn_v2_settlement_house_ground_entrance_matrix" "settlement_house"
$summaries += Add-StructureCandidateSet $taxonomy "fn_v2_village_cluster_ground_entrance_matrix" "village_cluster"

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
    } | Sort-Object { Get-TilesetProperty $_ "owner" "" }, { Get-TilesetProperty $_ "role" "" }, { [int](Get-TilesetProperty $_ "variant" 0) })

$roadSheetPath = Join-Path $resolvedOutputDirectory "roads_v401_v404_full_tile_contact_sheet.png"
$riverSheetPath = Join-Path $resolvedOutputDirectory "river_stream_v401_v404_full_tile_contact_sheet.png"
$bridgeSheetPath = Join-Path $resolvedOutputDirectory "bridge_spans_v401_v403_matching_river_contact_sheet.png"
$structureSheetPath = Join-Path $resolvedOutputDirectory "structures_v401_v402_family_contexts_review.png"
Draw-CandidateContactSheet (@($generatedRoadAssets | Select-Object -First 96)) $roadSheetPath "Road and trail v401-v404 full tile samples" 8
Draw-CandidateContactSheet $generatedRiverAssets $riverSheetPath "River stream v401-v404 integrated bank samples" 8
Draw-CandidateContactSheet $generatedBridgeAssets $bridgeSheetPath "Bridge spans v401-v403 matching river palette" 6
Draw-CandidateContactSheet (@($generatedStructureAssets | Select-Object -First 96)) $structureSheetPath "Structure families v401-v402 contextual samples" 6

$summaryPath = Join-Path $resolvedOutputDirectory "map_art_v401_cohesive_assets_summary.json"
Write-CleanJsonFile $summaryPath ([pscustomobject]@{
        schema_version = 1
        generated_at_local = (Get-Date).ToString("yyyy-MM-ddTHH:mm:ssK")
        status = "candidate_only_needs_in_app_user_feedback"
        tag = $reviewTag
        generated_work_orders = $summaries
        review_sheets = @(
            ConvertTo-TilesetRelativePath $roadSheetPath,
            ConvertTo-TilesetRelativePath $riverSheetPath,
            ConvertTo-TilesetRelativePath $bridgeSheetPath,
            ConvertTo-TilesetRelativePath $structureSheetPath
        )
        notes = @(
            "Replaces active road, trail, river, bridge, and structure candidates with v401 full-tile candidates.",
            "Adds separate settlement_house and village_cluster structure-family work orders so settlements and buildings no longer resolve as guard towers.",
            "Roads, trails, rivers, bridges, and structures now carry opaque matched ground instead of relying on green or black fallback tile backgrounds.",
            "Rivers use the same bank/water palette under bridges and avoid circular source blobs.",
            "No assets are marked accepted."
        )
    })

Write-Host "Generated v401 cohesive map-art candidates:"
foreach ($summary in $summaries) {
    Write-Host "  $($summary.work_order_id): $($summary.generated_asset_count) asset(s)"
}
Write-Host "Summary: $summaryPath"

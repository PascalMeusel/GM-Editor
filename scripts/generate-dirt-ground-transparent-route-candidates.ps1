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

$reviewTag = "map_art_v911_exact_v401_transparent_dirt_routes_20260620"
$reviewTagsToReplace = @(
    "map_art_v701_muted_dirt_ground_20260620",
    "map_art_v801_transparent_dirt_routes_20260620",
    "map_art_v901_v401_weight_transparent_dirt_routes_20260620",
    $reviewTag
)
$groundOwner = "dirt"

function Write-CleanJsonFile([string]$Path, $Value) {
    $json = $Value | ConvertTo-Json -Depth 48
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

function Save-PngBitmap([Drawing.Bitmap]$Bitmap, [string]$Path) {
    $directory = Split-Path -Parent $Path
    if ($directory) {
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
    }
    $tempPath = "$Path.tmp"
    if (Test-Path -LiteralPath $tempPath) {
        Remove-Item -LiteralPath $tempPath -Force
    }
    $Bitmap.Save($tempPath, [Drawing.Imaging.ImageFormat]::Png)
    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Force
    }
    Move-Item -LiteralPath $tempPath -Destination $Path -Force
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

function Clamp-Channel([int]$Value) {
    return [Math]::Max(0, [Math]::Min(255, $Value))
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

function Draw-OffsetPath($Graphics, [Drawing.Drawing2D.GraphicsPath]$Path, [string]$Direction, [double]$Offset, [Drawing.Pen]$Pen) {
    $vector = Get-DirectionVector $Direction
    $matrix = [Drawing.Drawing2D.Matrix]::new()
    $clone = $Path.Clone()
    try {
        $matrix.Translate([float](-[double]$vector.Y * $Offset), [float]([double]$vector.X * $Offset))
        $clone.Transform($matrix)
        $Graphics.DrawPath($Pen, $clone)
    }
    finally {
        $clone.Dispose()
        $matrix.Dispose()
    }
}

function Get-WayPens([string]$Kind) {
    if ($Kind -eq "road") {
        return [pscustomobject]@{
            compression = New-RoundPen (New-Color 79 55 32 170) 47.0
            body = New-RoundPen (New-Color 155 106 58 238) 35.0
            dust = New-RoundPen (New-Color 188 134 76 72) 13.0
            rut = New-RoundPen (New-Color 88 57 31 96) 2.0
            pebble = New-Color 78 55 34 68
            light = New-Color 205 163 96 58
        }
    }

    return [pscustomobject]@{
        compression = New-RoundPen (New-Color 74 55 32 135) 25.0
        body = New-RoundPen (New-Color 139 95 52 218) 16.0
        dust = New-RoundPen (New-Color 225 184 115 62) 4.0
        rut = New-RoundPen (New-Color 80 54 32 88) 1.3
        pebble = New-Color 82 60 39 54
        light = New-Color 203 162 97 46
    }
}

function Dispose-WayPens($Pens) {
    foreach ($name in @("compression", "body", "dust", "rut")) {
        if ($null -ne $Pens.$name) {
            $Pens.$name.Dispose()
        }
    }
}

function Draw-WayPath($Graphics, [Drawing.Drawing2D.GraphicsPath]$Path, [string]$Direction, $Pens, [string]$Kind) {
    $Graphics.DrawPath($Pens.compression, $Path)
    $Graphics.DrawPath($Pens.body, $Path)
    $Graphics.DrawPath($Pens.dust, $Path)
    if ($Kind -eq "road") {
        Draw-OffsetPath $Graphics $Path $Direction -7.0 $Pens.rut
        Draw-OffsetPath $Graphics $Path $Direction 7.0 $Pens.rut
    }
    else {
        $Graphics.DrawPath($Pens.rut, $Path)
    }
}

function Draw-WayNetwork($Graphics, [int]$Mask, [int]$Variant, [string]$Kind) {
    $directions = @(Get-MaskDirections $Mask)
    if ($directions.Count -eq 0) {
        return
    }

    $centerValue = $TileSize / 2.0
    $center = [Drawing.PointF]::new(
        [float]($centerValue + (($Variant % 2) - 0.5) * 1.1),
        [float]($centerValue + ((($Mask + $Variant) % 3) - 1) * 0.8))

    $pens = Get-WayPens $Kind
    try {
        if ($directions.Count -eq 2 -and (Get-OppositeDirection $directions[0]) -eq $directions[1]) {
            $start = Get-ExitPoint $directions[0] $TileSize
            $end = Get-ExitPoint $directions[1] $TileSize
            $path = New-PathCurve $start $end $directions[1] (($Mask * 89) + $Variant) 4.8
            try {
                Draw-WayPath $Graphics $path $directions[1] $pens $Kind
            }
            finally {
                $path.Dispose()
            }
            return
        }

        foreach ($direction in $directions) {
            $end = Get-ExitPoint $direction $TileSize
            $strength = if ($Kind -eq "road") { 4.5 } else { 6.0 }
            $path = New-PathCurve $center $end $direction (($Mask * 101) + ($Variant * 19) + (Get-StableStringSeed $direction)) $strength
            try {
                Draw-WayPath $Graphics $path $direction $pens $Kind
            }
            finally {
                $path.Dispose()
            }
        }
    }
    finally {
        Dispose-WayPens $pens
    }
}

function Draw-V401Branch($Graphics, [Drawing.Pen]$Pen, [Drawing.PointF]$Center, [string]$Direction, [int]$Seed, [double]$CurveStrength) {
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

function Draw-V401ContinuousPair($Graphics, [Drawing.Pen]$Pen, [string]$A, [string]$B, [int]$Seed, [double]$CurveStrength) {
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

function Draw-V401WayNetwork($Graphics, [int]$Mask, [int]$Variant, [Drawing.Pen[]]$Pens, [double]$CurveStrength) {
    $directions = @(Get-MaskDirections $Mask)
    if ($directions.Count -eq 0) {
        return
    }

    $centerValue = $TileSize / 2.0
    $center = [Drawing.PointF]::new(
        [float]($centerValue + (($Variant % 3) - 1) * 0.8),
        [float]($centerValue + ((($Mask + $Variant) % 3) - 1) * 0.8))

    if ($directions.Count -eq 2 -and (Get-OppositeDirection $directions[0]) -eq $directions[1]) {
        foreach ($pen in $Pens) {
            Draw-V401ContinuousPair $Graphics $pen $directions[0] $directions[1] (($Mask * 101) + $Variant) $CurveStrength
        }
        return
    }

    foreach ($pen in $Pens) {
        foreach ($direction in $directions) {
            Draw-V401Branch $Graphics $pen $center $direction (($Mask * 103) + ($Variant * 17) + (Get-StableStringSeed $direction)) $CurveStrength
        }
    }
}

function Draw-V401FlowMarks($Graphics, [int]$Mask, [int]$Variant, [Drawing.Color]$Color, [float]$Width) {
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

function Add-WayTexture($Graphics, [int]$Mask, [int]$Variant, [string]$Kind) {
    $random = [Random]::new((Get-RandomSeed (($Mask * 6101) + ($Variant * 83) + (Get-StableStringSeed $Kind))))
    $count = if ($Kind -eq "road") { 30 + ($Mask % 13) } else { 18 + ($Mask % 9) }
    $brush = [Drawing.SolidBrush]::new((New-Color 80 55 34 56))
    try {
        for ($i = 0; $i -lt $count; ++$i) {
            $brush.Color = if (($i % 4) -eq 0) { New-Color 184 143 88 42 } else { New-Color 70 50 32 54 }
            $w = if ($Kind -eq "road") { $random.Next(2, 6) } else { $random.Next(1, 4) }
            $h = $random.Next(1, 3)
            $x = $random.Next(12, $TileSize - 12)
            $y = $random.Next(12, $TileSize - 12)
            $Graphics.FillEllipse($brush, [Drawing.Rectangle]::new($x, $y, $w, $h))
        }
    }
    finally {
        $brush.Dispose()
    }
}

function Add-SubtlePixelBlend([Drawing.Bitmap]$Bitmap, [int]$Seed) {
    $random = [Random]::new((Get-RandomSeed $Seed))
    for ($y = 0; $y -lt $Bitmap.Height; ++$y) {
        for ($x = 0; $x -lt $Bitmap.Width; ++$x) {
            $pixel = $Bitmap.GetPixel($x, $y)
            $jitter = $random.Next(-3, 4)
            $r = Clamp-Channel ($pixel.R + $jitter)
            $g = Clamp-Channel ($pixel.G + [int]($jitter * 0.8))
            $b = Clamp-Channel ($pixel.B + [int]($jitter * 0.6))
            $Bitmap.SetPixel($x, $y, [Drawing.Color]::FromArgb(255, $r, $g, $b))
        }
    }
}

function New-V401WeightedTransparentWayPng([int]$Mask, [int]$Variant, [string]$Kind, [string]$OutputPath) {
    $bitmap = [Drawing.Bitmap]::new($TileSize, $TileSize, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $edgePen = $null
    $bodyPen = $null
    $rutPen = $null
    try {
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.Clear([Drawing.Color]::Transparent)
        if ($Kind -eq "trail") {
            $edgePen = New-RoundPen (New-Color 74 55 32 135) 25.0
            $bodyPen = New-RoundPen (New-Color 139 95 52 235) 16.0
            Draw-V401WayNetwork $graphics $Mask $Variant @($edgePen, $bodyPen) 6.0
            Draw-V401FlowMarks $graphics $Mask $Variant (New-Color 225 184 115 65) 1.2
        }
        else {
            $edgePen = New-RoundPen (New-Color 79 55 32 170) 47.0
            $bodyPen = New-RoundPen (New-Color 155 106 58 244) 35.0
            $rutPen = New-RoundPen (New-Color 88 57 31 105) 2.0
            Draw-V401WayNetwork $graphics $Mask $Variant @($edgePen, $bodyPen) 4.5
            Draw-V401WayNetwork $graphics $Mask ($Variant + 19) @($rutPen) 2.0
        }
        Save-PngBitmap $bitmap $OutputPath
    }
    finally {
        if ($rutPen) { $rutPen.Dispose() }
        if ($bodyPen) { $bodyPen.Dispose() }
        if ($edgePen) { $edgePen.Dispose() }
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function Get-AnchorWindow([string]$Direction, [int]$Size) {
    $center = [int]($Size / 2)
    $half = 24
    $depth = 11
    $corner = 22
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

function Get-PixelDelta([Drawing.Color]$A, [Drawing.Color]$B) {
    return [Math]::Abs([int]$A.R - [int]$B.R) +
        [Math]::Abs([int]$A.G - [int]$B.G) +
        [Math]::Abs([int]$A.B - [int]$B.B)
}

function Get-AnchorCoverage([Drawing.Bitmap]$GeneratedBitmap, [string]$Direction, [int]$Threshold) {
    $window = Get-AnchorWindow $Direction $GeneratedBitmap.Width
    $total = 0
    $covered = 0
    for ($y = $window.Y; $y -lt ($window.Y + $window.Height); ++$y) {
        for ($x = $window.X; $x -lt ($window.X + $window.Width); ++$x) {
            ++$total
            if ($GeneratedBitmap.GetPixel($x, $y).A -ge $Threshold) {
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
            $presentMin = if ($asset.kind -eq "road") { 0.095 } else { 0.04 }
            $absentMax = if ($asset.kind -eq "road") { 0.105 } else { 0.07 }
            $threshold = if ($asset.kind -eq "road") { 22 } else { 18 }
            foreach ($direction in $directions) {
                $coverage = Get-AnchorCoverage $bitmap $direction $threshold
                $expected = $direction -in $present
                $status = "ok"
                if ($expected -and $coverage -lt $presentMin) {
                    $status = "fail"
                    $errors += [pscustomobject]@{
                        issue = "v401_weight_transparent_dirt_way_anchor_missing"
                        asset = $asset.id
                        mask = Format-MaskToken $asset.mask
                        variant = $asset.variant
                        direction = $direction
                        coverage = [Math]::Round($coverage, 3)
                        required = $presentMin
                    }
                }
                if ((-not $expected) -and $coverage -gt $absentMax) {
                    $status = "fail"
                    $errors += [pscustomobject]@{
                        issue = "v401_weight_transparent_dirt_way_anchor_leak"
                        asset = $asset.id
                        mask = Format-MaskToken $asset.mask
                        variant = $asset.variant
                        direction = $direction
                        coverage = [Math]::Round($coverage, 3)
                        maximum = $absentMax
                    }
                }
                $anchors += [pscustomobject]@{
                    asset = $asset.id
                    kind = $asset.kind
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

function Draw-Label($Graphics, [string]$Text, [int]$X, [int]$Y, [int]$Width = 320) {
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

function Get-AssetImagePath($AssetOrGenerated) {
    if ($null -ne $AssetOrGenerated.path) {
        return $AssetOrGenerated.path
    }
    return Resolve-TilesetPath (Get-TilesetProperty $AssetOrGenerated "source_path" "")
}

function Draw-RouteReview($Graphics, $Grid, [int]$CellSize, [int]$OffsetY, $RouteAssets, $BaseAssets, [string]$Title, [int]$Salt) {
    Draw-Label $Graphics $Title 8 ($OffsetY + 6) 520
    $routeMaskUsage = @{}
    $missingRouteMasks = @{}
    for ($y = 0; $y -lt $Grid.Count; ++$y) {
        for ($x = 0; $x -lt $Grid[$y].Count; ++$x) {
            $dest = [Drawing.Rectangle]::new($x * $CellSize, $OffsetY + 30 + ($y * $CellSize), $CellSize, $CellSize)
            if (Test-RouteCell $Grid $x $y) {
                $baseAsset = $BaseAssets[(Get-TilesetStableIndex $x $y (8101 + $Salt) $BaseAssets.Count)]
                Draw-TilesetAsset $Graphics $baseAsset $dest
                $mask = Get-RouteMaskForGrid $Grid $x $y
                $maskToken = Format-MaskToken $mask
                if (-not $routeMaskUsage.ContainsKey($maskToken)) { $routeMaskUsage[$maskToken] = 0 }
                $routeMaskUsage[$maskToken] += 1
                $pool = @($RouteAssets | Where-Object {
                    if ($_.PSObject.Properties["mask"]) { [int]$_.mask -eq $mask }
                    else { [int](Get-TilesetProperty $_ "mask" -1) -eq $mask }
                } | Sort-Object id)
                if ($pool.Count -eq 0) {
                    $missingRouteMasks[$maskToken] = $true
                    $brush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(155, 130, 35, 45))
                    try { $Graphics.FillRectangle($brush, $dest) }
                    finally { $brush.Dispose() }
                }
                else {
                    $asset = $pool[(Get-TilesetStableIndex $x $y $Salt $pool.Count)]
                    $path = Get-AssetImagePath $asset
                    $image = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $path).Path)
                    try { $Graphics.DrawImage($image, $dest, 0, 0, $image.Width, $image.Height, [Drawing.GraphicsUnit]::Pixel) }
                    finally { $image.Dispose() }
                }
            }
            else {
                $baseAsset = $BaseAssets[(Get-TilesetStableIndex $x $y (8101 + $Salt) $BaseAssets.Count)]
                Draw-TilesetAsset $Graphics $baseAsset $dest
            }
        }
    }
    return [pscustomobject]@{
        route_mask_usage = $routeMaskUsage
        missing_route_masks = @($missingRouteMasks.Keys | Sort-Object)
    }
}

function New-RouteGrid {
    $routeWidth = 28
    $routeHeight = 14
    $grid = New-BoolGrid $routeWidth $routeHeight
    Add-RouteCells $grid @(@(1,2), @(2,2), @(3,2), @(4,2), @(5,2), @(6,2), @(7,2), @(8,2), @(9,2))
    Add-RouteCells $grid @(@(13,1), @(13,2), @(13,3), @(13,4), @(13,5), @(13,6), @(13,7))
    Add-RouteCells $grid @(@(2,6), @(3,7), @(4,8), @(5,9), @(6,10), @(7,11))
    Add-RouteCells $grid @(@(25,6), @(24,7), @(23,8), @(22,9), @(21,10), @(20,11))
    Add-RouteCells $grid @(@(16,2), @(17,2), @(18,2), @(19,2), @(20,2), @(20,3), @(20,4), @(20,5), @(20,6))
    Add-RouteCells $grid @(@(10,9), @(10,10), @(10,11), @(10,12), @(11,12), @(12,12), @(13,12))
    return $grid
}

function Get-WorkOrderSpec($Ways, [string]$WorkOrderId, [string]$Slug, [string]$Owner, [string]$Kind, [int]$VariantBase) {
    $workOrder = $Ways.production_work_orders | Where-Object { (Get-TilesetProperty $_ "id" "") -eq $WorkOrderId } | Select-Object -First 1
    if ($null -eq $workOrder) {
        throw "Missing work order '$WorkOrderId'."
    }
    $targetMasks = @(
        ConvertTo-TilesetArray (Get-TilesetProperty $workOrder "required_masks_or_roles" @()) |
            ForEach-Object { ConvertFrom-MaskToken ([string]$_) } |
            Where-Object { $null -ne $_ } |
            Sort-Object
    )
    return [pscustomobject]@{
        work_order_id = $WorkOrderId
        slug = $Slug
        owner = $Owner
        kind = $Kind
        variant_base = $VariantBase
        target_variants = [Math]::Max(1, [int](Get-TilesetProperty $workOrder "target_variants" 2))
        target_masks = $targetMasks
        directory = Join-Path $resolvedSourceRoot "way_segment/$Slug"
    }
}

function Add-V401WeightedTransparentWayCandidateSet($Ways, $Spec, $BaseAssets) {
    New-Item -ItemType Directory -Force -Path $Spec.directory | Out-Null
    $generated = @()
    foreach ($mask in $Spec.target_masks) {
        for ($variantIndex = 1; $variantIndex -le $Spec.target_variants; ++$variantIndex) {
            $variant = $Spec.variant_base + $variantIndex
            $maskToken = Format-MaskToken $mask
            $variantToken = Format-VariantToken $variant
            $fileName = "fantasy_neutral__world__way_segment__$($Spec.slug)`__$maskToken`__$variantToken.png"
            $outputPath = Join-Path $Spec.directory $fileName
            $baseAsset = $BaseAssets[(Get-TilesetStableIndex $mask $variant (9127 + $Spec.variant_base) $BaseAssets.Count)]
            New-V401WeightedTransparentWayPng $mask $variant $Spec.kind $outputPath
            $generated += [pscustomobject]@{
                id = "fantasy_neutral.world.way_segment.$($Spec.slug).$maskToken.$variantToken"
                work_order_id = $Spec.work_order_id
                slug = $Spec.slug
                owner = $Spec.owner
                kind = $Spec.kind
                mask = $mask
                mask_token = $maskToken
                variant = $variant
                variant_token = $variantToken
                path = $outputPath
                source_path = ConvertTo-TilesetRelativePath $outputPath
            base_path = Resolve-TilesetPath (Get-TilesetProperty $baseAsset "source_path" "")
            }
        }
    }

    $newManifestAssets = @()
    foreach ($asset in $generated) {
        $surface = if ($Spec.kind -eq "road") {
            "transparent exact-v401 compacted roadbed route layer over the selected dirt base so surrounding ground texture remains continuous"
        }
        else {
            "transparent exact-v401 worn trail route layer over the selected dirt base so surrounding ground texture remains continuous"
        }
        $newManifestAssets += [pscustomobject]@{
            id = $asset.id
            style = "fantasy_neutral"
            scale = "world"
            asset_class = "way_segment"
            owner = $Spec.owner
            work_order_id = $Spec.work_order_id
            mask = $asset.mask
            variant = $asset.variant
            source_path = $asset.source_path
            dimensions = [pscustomobject]@{ width = $TileSize; height = $TileSize }
            tile_shape = "square"
            transparent = $true
            edge_contract = "map_art_v911_exact_v401_transparent_dirt_route_alpha_anchor_contract"
            tags = @(
                "runtime_review_candidate",
                $reviewTag,
                "integrated_way_ground",
                $groundOwner,
                "mask_anchor_aligned",
                "exact_v401_style_transfer",
                "transparent_route_layer",
                "ground_texture_preserving",
                "candidate_only"
            )
            context = [pscustomobject]@{
                way_family = $Spec.owner
                ground = $groundOwner
                surface_interaction = $surface
                mask_mapping_status = "v911_exact_v401_transparent_dirt_route_anchor_contract_passed"
            }
            status = "candidate"
            validation = [pscustomobject]@{
                human_review = "needs_user_review"
                notes = "Exact-v401 transparent dirt-ground route repaint candidate: preserves the v401 route geometry, palette, and readability while removing the baked dirt square so the normal dirt terrain remains continuous. Candidate-only until app-scale review and user acceptance."
            }
        }
    }
    $Ways.assets = @($Ways.assets) + $newManifestAssets
    return $generated
}

function Write-ContactSheet($GeneratedAssets, $Spec) {
    $contactCell = 70
    $labelWidth = 92
    $rowHeight = $contactCell + 22
    $firstVariant = $Spec.variant_base + 1
    $lastVariant = $Spec.variant_base + $Spec.target_variants
    $contactPath = Join-Path $resolvedOutputDirectory "$($Spec.slug)_transparent_v$firstVariant`_v$lastVariant`_contact_sheet.png"
    $contact = New-TilesetReviewBitmap ($labelWidth + ($Spec.target_variants * $contactCell) + 24) (34 + ($Spec.target_masks.Count * $rowHeight) + 12)
    try {
        Draw-Label $contact.Graphics "$($Spec.slug) transparent v$firstVariant-v$lastVariant" 8 8 450
        for ($row = 0; $row -lt $Spec.target_masks.Count; ++$row) {
            $mask = [int]$Spec.target_masks[$row]
            $y = 34 + ($row * $rowHeight)
            Draw-Label $contact.Graphics (Format-MaskToken $mask) 8 ($y + 18) 78
            for ($variantIndex = 1; $variantIndex -le $Spec.target_variants; ++$variantIndex) {
                $variant = $Spec.variant_base + $variantIndex
                $x = $labelWidth + (($variantIndex - 1) * $contactCell)
                $dest = [Drawing.Rectangle]::new($x, $y, $contactCell, $contactCell)
                $asset = $GeneratedAssets | Where-Object { $_.mask -eq $mask -and $_.variant -eq $variant } | Select-Object -First 1
                $baseImage = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $asset.base_path).Path)
                try {
                    $contact.Graphics.DrawImage($baseImage, $dest, 0, 0, $baseImage.Width, $baseImage.Height, [Drawing.GraphicsUnit]::Pixel)
                }
                finally {
                    $baseImage.Dispose()
                }
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
        Save-PngBitmap $contact.Bitmap $contactPath
    }
    finally {
        $contact.Graphics.Dispose()
        $contact.Bitmap.Dispose()
    }
    return $contactPath
}

$waysPath = Join-Path $resolvedSourceRoot "ways.json"
$ways = Get-Content -LiteralPath $waysPath -Raw | ConvertFrom-Json

$specs = @(
    Get-WorkOrderSpec $ways "fn_v2_way_dirt_road_on_dirt" "way_dirt_road_on_dirt" "way_dirt_road" "road" 910
    Get-WorkOrderSpec $ways "fn_v2_way_trail_narrow_on_dirt" "way_trail_narrow_on_dirt" "way_trail_narrow" "trail" 910
)

$bundle = Get-TilesetV2ManifestBundle $resolvedSourceRoot
$tileAssets = @(Get-TilesetManifestAssets $bundle.Tiles)
$dirtAssets = @($tileAssets | Where-Object {
    (Get-TilesetProperty $_ "owner" "") -eq $groundOwner -and
    (Get-TilesetProperty $_ "asset_class" "") -eq "base" -and
    (Get-TilesetProperty $_ "status" "") -in @("candidate", "accepted")
})
if ($dirtAssets.Count -eq 0) {
    throw "No active '$groundOwner' base assets found for dirt-ground way compositing."
}

$oldCandidateAssets = @{}
foreach ($spec in $specs) {
    $originalV401 = @($ways.assets | Where-Object {
        $tags = @(ConvertTo-TilesetArray (Get-TilesetProperty $_ "tags" @()))
        (Get-TilesetProperty $_ "work_order_id" "") -eq $spec.work_order_id -and
        "map_art_v401_cohesive_full_tile_20260531" -in $tags
    })
    $currentCandidates = @($ways.assets | Where-Object {
        (Get-TilesetProperty $_ "work_order_id" "") -eq $spec.work_order_id -and
        (Get-TilesetProperty $_ "status" "") -eq "candidate"
    })
    $oldCandidateAssets[$spec.work_order_id] = if ($originalV401.Count -gt 0) { $originalV401 } else { $currentCandidates }
}

$ways.assets = @($ways.assets | Where-Object {
    $tags = @(ConvertTo-TilesetArray (Get-TilesetProperty $_ "tags" @()))
    @($tags | Where-Object { $_ -in $reviewTagsToReplace }).Count -eq 0
})

foreach ($asset in (ConvertTo-TilesetArray (Get-TilesetProperty $ways "assets" @()))) {
    $workOrderId = Get-TilesetProperty $asset "work_order_id" ""
    if ($workOrderId -notin @("fn_v2_way_dirt_road_on_dirt", "fn_v2_way_trail_narrow_on_dirt")) {
        continue
    }
    if ((Get-TilesetProperty $asset "status" "") -ne "candidate") {
        continue
    }
    Set-JsonProperty $asset "status" "needs_repaint"
    Set-JsonProperty $asset "rejected_in_app_review" "2026-06-20 replaced by exact-v401 transparent dirt route-layer v911+ candidate"
    $tags = @(ConvertTo-TilesetArray (Get-TilesetProperty $asset "tags" @()))
    if ("superseded_by_exact_v401_transparent_dirt_routes_v911" -notin $tags) {
        Set-JsonProperty $asset "tags" @($tags + "superseded_by_exact_v401_transparent_dirt_routes_v911")
    }
    $validation = Get-TilesetProperty $asset "validation" ([pscustomobject]@{})
    Set-JsonProperty $validation "human_review" "needs_repaint"
    Set-JsonProperty $validation "notes" "Held after review: active dirt-ground way assets either read too hard-outlined/cartoon, too line-like, or carried a mismatched baked dirt ground. Superseded by exact-v401 transparent v911+ route-layer candidates."
    Set-JsonProperty $asset "validation" $validation
}

$generatedAssets = @()
$contactEvidence = @{}
foreach ($spec in $specs) {
    $set = Add-V401WeightedTransparentWayCandidateSet $ways $spec $dirtAssets
    $generatedAssets += $set
    $contactEvidence[$spec.work_order_id] = Write-ContactSheet $set $spec
}

$anchorReport = Test-GeneratedAnchorContract $generatedAssets
$anchorReportPath = Resolve-TilesetPath "build/dirt_ground_transparent_v911_anchor_report.json"
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $anchorReportPath) | Out-Null
Write-CleanJsonFile $anchorReportPath ([pscustomobject]@{
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    work_order_ids = @($specs | ForEach-Object { $_.work_order_id })
    variants = [pscustomobject]@{
        road = @(911..914)
        trail = @(911..912)
    }
    anchor_contract = "alpha 8-neighbor exact-v401 transparent dirt route-layer contract"
    summary = $anchorReport.summary
    errors = $anchorReport.errors
    anchors = $anchorReport.anchors
})
if ($anchorReport.summary.error_count -gt 0) {
    throw "Transparent dirt-ground route anchor validation failed with $($anchorReport.summary.error_count) issue(s). See $anchorReportPath."
}

$ways.assets = @($ways.assets)
Write-CleanJsonFile $waysPath $ways

$routeGrid = New-RouteGrid
$cellSize = 32
$routeWidth = $routeGrid[0].Count
$routeHeight = $routeGrid.Count
$sectionGap = 28
$sectionHeight = ($routeHeight * $cellSize) + 30
$reviewWidth = [Math]::Max(($routeWidth * $cellSize), 920)
$reviewHeight = ($sectionHeight * 4) + ($sectionGap * 3) + 60
$appScalePath = Join-Path $resolvedOutputDirectory "dirt_ground_ways_v401_vs_v911_app_scale_review.png"
$app = New-TilesetReviewBitmap $reviewWidth $reviewHeight
$routeSummaries = @{}
try {
    $sectionY = 0
    $routeSummaries["current_road"] = Draw-RouteReview $app.Graphics $routeGrid $cellSize $sectionY $oldCandidateAssets["fn_v2_way_dirt_road_on_dirt"] $dirtAssets "current dirt road candidates v401-v404 at 32px" 7101
    $sectionY += $sectionHeight + $sectionGap
    $routeSummaries["transparent_road_v911"] = Draw-RouteReview $app.Graphics $routeGrid $cellSize $sectionY (@($generatedAssets | Where-Object { $_.work_order_id -eq "fn_v2_way_dirt_road_on_dirt" })) $dirtAssets "exact-v401 transparent dirt road route candidates v911-v914 at 32px" 7251
    $sectionY += $sectionHeight + $sectionGap
    $routeSummaries["current_trail"] = Draw-RouteReview $app.Graphics $routeGrid $cellSize $sectionY $oldCandidateAssets["fn_v2_way_trail_narrow_on_dirt"] $dirtAssets "current dirt trail candidates v401-v402 at 32px" 7301
    $sectionY += $sectionHeight + $sectionGap
    $routeSummaries["transparent_trail_v911"] = Draw-RouteReview $app.Graphics $routeGrid $cellSize $sectionY (@($generatedAssets | Where-Object { $_.work_order_id -eq "fn_v2_way_trail_narrow_on_dirt" })) $dirtAssets "exact-v401 transparent dirt trail route candidates v911-v912 at 32px" 7451
    Draw-Label $app.Graphics "candidate-only: v911 keeps exact v401 route art but preserves the surrounding dirt texture" 8 ($reviewHeight - 42) 760
    Save-PngBitmap $app.Bitmap $appScalePath
}
finally {
    $app.Graphics.Dispose()
    $app.Bitmap.Dispose()
}

$reportPath = Join-Path $resolvedOutputDirectory "dirt_ground_ways_transparent_v911_review.json"
Write-CleanJsonFile $reportPath ([pscustomobject]@{
    schema_version = 1
    generated_at_local = (Get-Date).ToString("yyyy-MM-ddTHH:mm:ssK")
    status = "candidate_only_exact_v401_transparent_dirt_ground_routes_v911"
    generated_asset_count = $generatedAssets.Count
    replaced_active_candidate_status = "full-tile dirt-road/trail-on-dirt candidates removed or marked needs_repaint"
    work_orders = @($specs | ForEach-Object {
        $spec = $_
        [pscustomobject]@{
            id = $spec.work_order_id
            slug = $spec.slug
            generated_assets = @($generatedAssets | Where-Object { $_.work_order_id -eq $spec.work_order_id }).Count
            masks = @($spec.target_masks | ForEach-Object { Format-MaskToken $_ })
            variants = @(($spec.variant_base + 1)..($spec.variant_base + $spec.target_variants))
            contact_sheet = ConvertTo-TilesetRelativePath $contactEvidence[$spec.work_order_id]
        }
    })
    comparison_review_sheet = ConvertTo-TilesetRelativePath $appScalePath
    route_summaries = $routeSummaries
    evidence = [pscustomobject]@{
        comparison_review_sheet = ConvertTo-TilesetRelativePath $appScalePath
        anchor_report = ConvertTo-TilesetRelativePath $anchorReportPath
    }
    open_acceptance_holds = @(
        "candidate_only",
        "user_art_direction_review_required",
        "runtime_promotion_not_performed"
    )
})

Write-Host "Generated exact-v401 transparent dirt-ground route candidates:"
Write-Host "  Assets:        $($generatedAssets.Count)"
Write-Host "  Anchor errors: $($anchorReport.summary.error_count)"
Write-Host "  App review:    $appScalePath"
Write-Host "  Review:        $reportPath"
Write-Host "  Anchor report: $anchorReportPath"

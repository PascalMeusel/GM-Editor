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

$reviewTag = "bridge_bank_integrated_v501_20260620"
$variantStart = 501
$variantEnd = 503
$rolesInOrder = @("approach_a", "threshold_a", "mid_01", "mid_02", "threshold_b", "approach_b")

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

function New-Color([int]$R, [int]$G, [int]$B, [int]$A = 255) {
    return [Drawing.Color]::FromArgb($A, $R, $G, $B)
}

function New-RoundPen([Drawing.Color]$Color, [float]$Width) {
    $pen = [Drawing.Pen]::new($Color, $Width)
    $pen.StartCap = [Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [Drawing.Drawing2D.LineCap]::Round
    $pen.LineJoin = [Drawing.Drawing2D.LineJoin]::Round
    return $pen
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

function Invoke-WithBitmap([string]$Path, [scriptblock]$Body) {
    $bitmap = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $Path).Path)
    try {
        & $Body $bitmap
    }
    finally {
        $bitmap.Dispose()
    }
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

function Fill-Polygon($Graphics, [Drawing.Color]$Color, [Drawing.Point[]]$Points) {
    $brush = [Drawing.SolidBrush]::new($Color)
    try { $Graphics.FillPolygon($brush, $Points) }
    finally { $brush.Dispose() }
}

function Draw-TexturedBase($Graphics, [string]$Surface, [int]$Seed) {
    $palette = switch ($Surface) {
        "water" { @{ Base = New-Color 48 138 160; Light = New-Color 103 185 190 90; Dark = New-Color 28 93 124 95; Accent = New-Color 201 230 220 75 } }
        "stone_bank" { @{ Base = New-Color 125 120 103; Light = New-Color 158 151 128 88; Dark = New-Color 82 78 68 80; Accent = New-Color 180 169 132 70 } }
        "dirt_bank" { @{ Base = New-Color 116 96 71; Light = New-Color 151 126 88 92; Dark = New-Color 78 63 47 82; Accent = New-Color 171 147 99 72 } }
        default { @{ Base = New-Color 82 132 66; Light = New-Color 122 160 81 82; Dark = New-Color 50 86 46 70; Accent = New-Color 178 161 82 65 } }
    }

    $Graphics.Clear($palette.Base)
    $random = [Random]::new((Get-RandomSeed (7100 + [int64]$Seed + [int64](Get-StableStringSeed $Surface))))
    $brush = [Drawing.SolidBrush]::new($palette.Light)
    $pen = [Drawing.Pen]::new($palette.Accent, 1.3)
    try {
        $count = if ($Surface -eq "water") { 70 } else { 130 }
        for ($i = 0; $i -lt $count; ++$i) {
            $brush.Color = if (($i % 6) -eq 0) { $palette.Accent } elseif (($i % 2) -eq 0) { $palette.Dark } else { $palette.Light }
            $w = $random.Next(2, 8)
            $h = $random.Next(1, 4)
            $Graphics.FillEllipse($brush, [Drawing.Rectangle]::new($random.Next(-2, $TileSize), $random.Next(-2, $TileSize), $w, $h))
        }
        if ($Surface -eq "water") {
            for ($i = 0; $i -lt 12; ++$i) {
                $y = $random.Next(5, $TileSize - 5)
                $x = $random.Next(0, $TileSize - 24)
                $Graphics.DrawLine($pen, $x, $y, $x + $random.Next(12, 28), $y + $random.Next(-1, 2))
            }
        }
    }
    finally {
        $pen.Dispose()
        $brush.Dispose()
    }
}

function Draw-IrregularBank($Graphics, [string]$Role, [string]$Family, [int]$Variant) {
    if ($Role -notin @("threshold_a", "threshold_b")) {
        return
    }

    $topBank = $Role -eq "threshold_a"
    $surface = if ($Family -eq "bridge_stone") { "stone_bank" } else { "grass" }
    $baseColor = if ($surface -eq "stone_bank") { New-Color 124 116 96 } else { New-Color 78 130 63 }
    $edgeColor = if ($surface -eq "stone_bank") { New-Color 78 73 62 185 } else { New-Color 63 86 45 175 }
    $wetColor = New-Color 35 92 110 105
    $seedOffset = (($Variant - 500) * 3)
    $curve = @(
        [Drawing.Point]::new(0, 39 + (($seedOffset + 2) % 7)),
        [Drawing.Point]::new(23, 35 + (($seedOffset + 5) % 9)),
        [Drawing.Point]::new(48, 43 + (($seedOffset + 1) % 7)),
        [Drawing.Point]::new(74, 36 + (($seedOffset + 4) % 8)),
        [Drawing.Point]::new(102, 41 + (($seedOffset + 7) % 7)),
        [Drawing.Point]::new($TileSize, 37 + (($seedOffset + 3) % 8))
    )

    if ($topBank) {
        $points = @(
            [Drawing.Point]::new(0, 0),
            [Drawing.Point]::new($TileSize, 0),
            $curve[5],
            $curve[4],
            $curve[3],
            $curve[2],
            $curve[1],
            $curve[0]
        )
        Fill-Polygon $Graphics $baseColor ([Drawing.Point[]]$points)
    }
    else {
        $flipped = @($curve | ForEach-Object { [Drawing.Point]::new($_.X, $TileSize - $_.Y) })
        $points = @(
            [Drawing.Point]::new(0, $TileSize),
            [Drawing.Point]::new($TileSize, $TileSize),
            $flipped[5],
            $flipped[4],
            $flipped[3],
            $flipped[2],
            $flipped[1],
            $flipped[0]
        )
        Fill-Polygon $Graphics $baseColor ([Drawing.Point[]]$points)
        $curve = $flipped
    }

    $edgePen = [Drawing.Pen]::new($edgeColor, 3.2)
    $wetPen = [Drawing.Pen]::new($wetColor, 2.0)
    try {
        for ($i = 0; $i -lt ($curve.Count - 1); ++$i) {
            $Graphics.DrawLine($edgePen, $curve[$i], $curve[$i + 1])
            $wetA = [Drawing.Point]::new($curve[$i].X, $curve[$i].Y + $(if ($topBank) { 4 } else { -4 }))
            $wetB = [Drawing.Point]::new($curve[$i + 1].X, $curve[$i + 1].Y + $(if ($topBank) { 4 } else { -4 }))
            $Graphics.DrawLine($wetPen, $wetA, $wetB)
        }
    }
    finally {
        $wetPen.Dispose()
        $edgePen.Dispose()
    }
}

function Draw-DirtApproach($Graphics, [string]$Role, [int]$Variant) {
    $edgePen = New-RoundPen (New-Color 82 58 36 150) 51.0
    $bodyPen = New-RoundPen (New-Color 143 96 55 236) 36.0
    $rutPen = New-RoundPen (New-Color 74 50 30 90) 2.0
    try {
        $Graphics.DrawLine($edgePen, 64, -8, 64, $TileSize + 8)
        $Graphics.DrawLine($bodyPen, 64, -8, 64, $TileSize + 8)
        $x1 = 55 + (($Variant - 501) % 2)
        $x2 = 73 - (($Variant - 501) % 2)
        $Graphics.DrawLine($rutPen, $x1, -4, $x1 + 3, $TileSize + 4)
        $Graphics.DrawLine($rutPen, $x2, -4, $x2 - 3, $TileSize + 4)
    }
    finally {
        $rutPen.Dispose()
        $bodyPen.Dispose()
        $edgePen.Dispose()
    }

    $startY = if ($Role -eq "approach_a") { 86 } else { -4 }
    $height = 46
    Draw-WoodDeck $Graphics $Role $Variant $startY ($startY + $height) -ApproachApron
}

function Draw-StoneApproach($Graphics, [string]$Role, [int]$Variant) {
    $edgePen = New-RoundPen (New-Color 55 53 47 200) 56.0
    $bodyPen = New-RoundPen (New-Color 156 151 132 246) 40.0
    $mortarPen = [Drawing.Pen]::new((New-Color 60 59 53 130), 1.7)
    $sidePen = [Drawing.Pen]::new((New-Color 45 43 38 155), 2.0)
    try {
        $Graphics.DrawLine($edgePen, 64, -8, 64, $TileSize + 8)
        $Graphics.DrawLine($bodyPen, 64, -8, 64, $TileSize + 8)
        $Graphics.DrawLine($sidePen, 43, -4, 43, $TileSize + 4)
        $Graphics.DrawLine($sidePen, 85, -4, 85, $TileSize + 4)
        for ($y = -8 + (($Variant - 500) * 5 % 15); $y -lt $TileSize + 16; $y += 15) {
            $Graphics.DrawLine($mortarPen, 44, $y, 84, $y)
            $offset = if ((([int](($y + 100) / 15)) % 2) -eq 0) { 0 } else { 11 }
            for ($x = 47 + $offset; $x -lt 84; $x += 21) {
                $Graphics.DrawLine($mortarPen, $x, $y, $x, $y + 15)
            }
        }
    }
    finally {
        $sidePen.Dispose()
        $mortarPen.Dispose()
        $bodyPen.Dispose()
        $edgePen.Dispose()
    }

    $startY = if ($Role -eq "approach_a") { 88 } else { -4 }
    Draw-StoneDeck $Graphics $Role $Variant $startY ($startY + 44) -ApproachApron
}

function Draw-WoodDeck($Graphics, [string]$Role, [int]$Variant, [int]$Top = -4, [int]$Bottom = 132, [switch]$ApproachApron) {
    $deckWidth = 46 + (($Variant - 502) * 2)
    $left = [int](64 - ($deckWidth / 2))
    $height = $Bottom - $Top
    $deck = @(
        (New-Color 132 82 42 248),
        (New-Color 145 91 47 248),
        (New-Color 119 74 39 248)
    )[($Variant - 501) % 3]
    $shadow = New-Color 35 24 18 92
    $dark = New-Color 70 43 25 150
    $rail = New-Color 69 45 28 225
    $rope = New-Color 174 132 79 170

    Fill-Rect $Graphics $shadow ($left - 8) $Top ($deckWidth + 16) $height
    Fill-Rect $Graphics $deck $left $Top $deckWidth $height

    $linePen = [Drawing.Pen]::new($dark, 2.6)
    $railPen = [Drawing.Pen]::new($rail, $(if ($ApproachApron) { 3.0 } else { 4.8 }))
    $ropePen = [Drawing.Pen]::new($rope, 1.6)
    try {
        $step = if ($Role -eq "mid_02") { 19 } else { 16 }
        $offset = (($Variant - 500) * 5 + $(if ($Role -eq "mid_02") { 8 } else { 0 })) % $step
        for ($y = $Top - $offset; $y -lt $Bottom + $step; $y += $step) {
            $Graphics.DrawLine($linePen, $left + 2, $y, $left + $deckWidth - 2, $y + (($Variant + $y) % 3))
        }

        $Graphics.DrawLine($railPen, $left - 6, $Top, $left - 5, $Bottom)
        $Graphics.DrawLine($railPen, $left + $deckWidth + 6, $Top, $left + $deckWidth + 5, $Bottom)
        if (-not $ApproachApron) {
            $Graphics.DrawLine($ropePen, $left - 13, $Top, $left - 13, $Bottom)
            $Graphics.DrawLine($ropePen, $left + $deckWidth + 13, $Top, $left + $deckWidth + 13, $Bottom)
        }
    }
    finally {
        $ropePen.Dispose()
        $railPen.Dispose()
        $linePen.Dispose()
    }

    if ($Role -in @("threshold_a", "threshold_b")) {
        $y = if ($Role -eq "threshold_a") { 31 } else { 81 }
        Fill-Rect $Graphics (New-Color 83 76 63 232) ($left - 22) $y ($deckWidth + 44) 16
        $capPen = [Drawing.Pen]::new((New-Color 54 43 32 170), 2.0)
        try { $Graphics.DrawRectangle($capPen, [Drawing.Rectangle]::new($left - 22, $y, $deckWidth + 44, 16)) }
        finally { $capPen.Dispose() }
    }

    if ($Role -eq "mid_02") {
        $bracePen = [Drawing.Pen]::new((New-Color 61 38 23 168), 3.5)
        try {
            $Graphics.DrawLine($bracePen, $left + 4, 14, $left + $deckWidth - 4, 56)
            $Graphics.DrawLine($bracePen, $left + $deckWidth - 4, 72, $left + 4, 116)
        }
        finally { $bracePen.Dispose() }
    }
}

function Draw-StoneDeck($Graphics, [string]$Role, [int]$Variant, [int]$Top = -4, [int]$Bottom = 132, [switch]$ApproachApron) {
    $deckWidth = 54 + (($Variant - 502) * 2)
    $left = [int](64 - ($deckWidth / 2))
    $height = $Bottom - $Top
    $stone = @(
        (New-Color 128 126 116 246),
        (New-Color 145 141 128 246),
        (New-Color 116 116 109 246)
    )[($Variant - 501) % 3]
    $edge = New-Color 70 68 62 225
    $mortar = New-Color 52 52 48 125

    Fill-Rect $Graphics (New-Color 35 34 31 65) ($left - 7) $Top ($deckWidth + 14) $height
    Fill-Rect $Graphics $stone $left $Top $deckWidth $height

    $edgePen = [Drawing.Pen]::new($edge, $(if ($ApproachApron) { 3.0 } else { 5.0 }))
    $mortarPen = [Drawing.Pen]::new($mortar, 1.8)
    try {
        $Graphics.DrawLine($edgePen, $left - 3, $Top, $left - 3, $Bottom)
        $Graphics.DrawLine($edgePen, $left + $deckWidth + 3, $Top, $left + $deckWidth + 3, $Bottom)
        $blockH = if ($Role -eq "mid_02") { 18 } else { 16 }
        $offset = (($Variant - 500) * 7 + $(if ($Role -eq "mid_02") { 9 } else { 0 })) % $blockH
        for ($y = $Top - $offset; $y -lt $Bottom + $blockH; $y += $blockH) {
            $Graphics.DrawLine($mortarPen, $left + 2, $y, $left + $deckWidth - 2, $y)
            $xOffset = if ((([int](($y + 200) / $blockH)) % 2) -eq 0) { 0 } else { 10 }
            for ($x = $left + 4 + $xOffset; $x -lt ($left + $deckWidth - 2); $x += 20) {
                $Graphics.DrawLine($mortarPen, $x, $y, $x, $y + $blockH)
            }
        }
    }
    finally {
        $mortarPen.Dispose()
        $edgePen.Dispose()
    }

    if ($Role -in @("threshold_a", "threshold_b")) {
        $y = if ($Role -eq "threshold_a") { 28 } else { 82 }
        Fill-Rect $Graphics (New-Color 91 88 78 242) ($left - 19) $y ($deckWidth + 38) 18
        $capPen = [Drawing.Pen]::new((New-Color 55 54 49 145), 2.0)
        try { $Graphics.DrawRectangle($capPen, [Drawing.Rectangle]::new($left - 19, $y, $deckWidth + 38, 18)) }
        finally { $capPen.Dispose() }
    }

    if ($Role -eq "mid_02") {
        $archPen = [Drawing.Pen]::new((New-Color 68 66 60 145), 3.0)
        try { $Graphics.DrawArc($archPen, [Drawing.Rectangle]::new($left + 6, 31, $deckWidth - 12, 56), 180, 180) }
        finally { $archPen.Dispose() }
    }
}

function Draw-BridgeAsset($Graphics, [string]$Family, [string]$Role, [int]$Variant) {
    $seed = (Get-StableStringSeed "$Family/$Role/$Variant")
    if ($Role -in @("mid_01", "mid_02", "threshold_a", "threshold_b")) {
        Draw-TexturedBase $Graphics "water" $seed
        Draw-IrregularBank $Graphics $Role $Family $Variant
    }
    else {
        $ground = if ($Family -eq "bridge_stone") { "dirt_bank" } else { "grass" }
        Draw-TexturedBase $Graphics $ground $seed
    }

    if ($Family -eq "bridge_stone") {
        if ($Role -in @("approach_a", "approach_b")) {
            Draw-StoneApproach $Graphics $Role $Variant
        }
        else {
            Draw-StoneDeck $Graphics $Role $Variant
        }
    }
    else {
        if ($Role -in @("approach_a", "approach_b")) {
            Draw-DirtApproach $Graphics $Role $Variant
        }
        else {
            Draw-WoodDeck $Graphics $Role $Variant
        }
    }
}

function New-BridgeIntegratedPng([string]$Family, [string]$Role, [int]$Variant, [string]$OutputPath) {
    $bitmap = [Drawing.Bitmap]::new($TileSize, $TileSize, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        Draw-BridgeAsset $graphics $Family $Role $Variant
        $bitmap.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function Get-RegionAverage([Drawing.Bitmap]$Bitmap, [Drawing.Rectangle]$Rect) {
    $r = 0.0
    $g = 0.0
    $b = 0.0
    $count = 0
    for ($y = $Rect.Y; $y -lt ($Rect.Y + $Rect.Height); ++$y) {
        for ($x = $Rect.X; $x -lt ($Rect.X + $Rect.Width); ++$x) {
            $p = $Bitmap.GetPixel($x, $y)
            $r += $p.R
            $g += $p.G
            $b += $p.B
            ++$count
        }
    }
    return [pscustomobject]@{ R = $r / $count; G = $g / $count; B = $b / $count }
}

function Get-ColorDistance($Left, $Right) {
    $dr = [double]$Left.R - [double]$Right.R
    $dg = [double]$Left.G - [double]$Right.G
    $db = [double]$Left.B - [double]$Right.B
    return [Math]::Sqrt(($dr * $dr) + ($dg * $dg) + ($db * $db))
}

function Test-RouteEdgeContrast($Generated) {
    $checks = [System.Collections.Generic.List[object]]::new()
    foreach ($asset in $Generated) {
        $bitmap = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $asset.path).Path)
        try {
            foreach ($edge in @("top", "bottom")) {
                $y = if ($edge -eq "top") { 0 } else { $bitmap.Height - 10 }
                $center = Get-RegionAverage $bitmap ([Drawing.Rectangle]::new(46, $y, 36, 10))
                $left = Get-RegionAverage $bitmap ([Drawing.Rectangle]::new(4, $y, 26, 10))
                $right = Get-RegionAverage $bitmap ([Drawing.Rectangle]::new(98, $y, 26, 10))
                $side = [pscustomobject]@{
                    R = ($left.R + $right.R) / 2.0
                    G = ($left.G + $right.G) / 2.0
                    B = ($left.B + $right.B) / 2.0
                }
                $distance = [Math]::Round((Get-ColorDistance $center $side), 2)
                [void]$checks.Add([pscustomobject]@{
                    id = $asset.id
                    role = $asset.role
                    variant = $asset.variant
                    edge = $edge
                    center_to_side_rgb_distance = $distance
                    status = if ($distance -ge 18.0) { "ok" } else { "review" }
                })
            }
        }
        finally {
            $bitmap.Dispose()
        }
    }
    return @($checks)
}

function Draw-Label($Graphics, [string]$Text, [int]$X, [int]$Y, [int]$Width = 260, [float]$Size = 9.0) {
    $font = [Drawing.Font]::new("Segoe UI", $Size)
    $brush = [Drawing.SolidBrush]::new((New-Color 235 238 242 246))
    $backBrush = [Drawing.SolidBrush]::new((New-Color 8 12 16 168))
    try {
        $Graphics.FillRectangle($backBrush, $X, $Y, $Width, [int]($Size + 8))
        $Graphics.DrawString($Text, $font, $brush, [Drawing.PointF]::new($X + 4, $Y + 2))
    }
    finally {
        $backBrush.Dispose()
        $brush.Dispose()
        $font.Dispose()
    }
}

function New-ReviewBitmap([int]$Width, [int]$Height) {
    $bitmap = [Drawing.Bitmap]::new($Width, $Height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
    $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.Clear((New-Color 24 25 24))
    return [pscustomobject]@{ Bitmap = $bitmap; Graphics = $graphics }
}

function Draw-ContactSheet($Assets, [string]$OutputPath) {
    $cell = 142
    $labelHeight = 24
    $header = 46
    $columns = 6
    $rows = [int][Math]::Ceiling($Assets.Count / [double]$columns)
    $sheet = New-ReviewBitmap (($columns * $cell) + 28) ($header + ($rows * ($cell + $labelHeight)) + 18)
    try {
        Draw-Label $sheet.Graphics "Bridge bank-integrated candidates v501-v503" 12 12 520 12.0
        for ($i = 0; $i -lt $Assets.Count; ++$i) {
            $asset = $Assets[$i]
            $col = $i % $columns
            $row = [int][Math]::Floor($i / $columns)
            $x = 14 + ($col * $cell)
            $y = $header + ($row * ($cell + $labelHeight))
            Invoke-WithBitmap $asset.path {
                param($bitmap)
                $sheet.Graphics.DrawImage($bitmap, [Drawing.Rectangle]::new($x + 7, $y, 128, 128), 0, 0, $bitmap.Width, $bitmap.Height, [Drawing.GraphicsUnit]::Pixel)
            }
            Draw-Label $sheet.Graphics "$($asset.owner) $($asset.role) $(Format-VariantToken $asset.variant)" $x ($y + 130) ($cell - 6) 7.4
        }
        $sheet.Bitmap.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $sheet.Graphics.Dispose()
        $sheet.Bitmap.Dispose()
    }
}

function Draw-SequenceSheet($Assets, [string]$OutputPath, [string]$Title) {
    $cell = 52
    $columnGap = 28
    $familyGap = 42
    $variantCount = ($variantEnd - $variantStart) + 1
    $width = 150 + ($variantCount * $cell) + (($variantCount - 1) * $columnGap) + 26
    $familyHeight = 36 + ($rolesInOrder.Count * $cell)
    $families = @($Assets | ForEach-Object { $_.owner } | Sort-Object -Unique)
    $height = 18 + ($families.Count * $familyHeight) + (($families.Count - 1) * $familyGap)
    $sheet = New-ReviewBitmap $width $height
    try {
        $yBase = 10
        foreach ($owner in $families) {
            Draw-Label $sheet.Graphics "$owner $Title" 8 $yBase 430 9.0
            $yBase += 28
            for ($variant = $variantStart; $variant -le $variantEnd; ++$variant) {
                $x = 150 + (($variant - $variantStart) * ($cell + $columnGap))
                Draw-Label $sheet.Graphics (Format-VariantToken $variant) $x $yBase $cell 8.0
                for ($row = 0; $row -lt $rolesInOrder.Count; ++$row) {
                    $role = $rolesInOrder[$row]
                    if ($variant -eq $variantStart) {
                        Draw-Label $sheet.Graphics $role 8 ($yBase + 20 + ($row * $cell) + 15) 120 7.5
                    }
                    $asset = $Assets | Where-Object { $_.owner -eq $owner -and $_.role -eq $role -and $_.variant -eq $variant } | Select-Object -First 1
                    if ($null -eq $asset) { continue }
                    $dest = [Drawing.Rectangle]::new($x, $yBase + 20 + ($row * $cell), $cell, $cell)
                    Invoke-WithBitmap $asset.path {
                        param($bitmap)
                        $sheet.Graphics.DrawImage($bitmap, $dest, 0, 0, $bitmap.Width, $bitmap.Height, [Drawing.GraphicsUnit]::Pixel)
                    }
                }
            }
            $yBase += 20 + ($rolesInOrder.Count * $cell) + $familyGap
        }
        $sheet.Bitmap.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $sheet.Graphics.Dispose()
        $sheet.Bitmap.Dispose()
    }
}

function Draw-ComparisonSheet($Ways, $Assets, [string]$OutputPath) {
    $cell = 52
    $columns = @(
        @{ Label = "v401 reference"; Variant = 401 },
        @{ Label = "v501 bank pass"; Variant = 501 },
        @{ Label = "v502 bank pass"; Variant = 502 },
        @{ Label = "v503 bank pass"; Variant = 503 }
    )
    $families = @($Assets | ForEach-Object { $_.owner } | Sort-Object -Unique)
    $width = 150 + ($columns.Count * 82) + 18
    $familyHeight = 36 + ($rolesInOrder.Count * $cell)
    $height = 18 + ($families.Count * $familyHeight) + (($families.Count - 1) * 42)
    $sheet = New-ReviewBitmap $width $height
    try {
        $yBase = 10
        foreach ($owner in $families) {
            Draw-Label $sheet.Graphics "$owner v401-v503 sequence comparison" 8 $yBase 450 9.0
            $yBase += 28
            for ($c = 0; $c -lt $columns.Count; ++$c) {
                $column = $columns[$c]
                $x = 150 + ($c * 82)
                Draw-Label $sheet.Graphics $column.Label $x $yBase 78 7.5
                for ($row = 0; $row -lt $rolesInOrder.Count; ++$row) {
                    $role = $rolesInOrder[$row]
                    if ($c -eq 0) {
                        Draw-Label $sheet.Graphics $role 8 ($yBase + 20 + ($row * $cell) + 15) 120 7.5
                    }
                    $asset = $null
                    if ($column.Variant -eq 401) {
                        $asset = $Ways.assets | Where-Object {
                            (Get-TilesetProperty $_ "owner" "") -eq $owner -and
                            (Get-TilesetProperty $_ "role" "") -eq $role -and
                            [int](Get-TilesetProperty $_ "variant" 0) -eq 401
                        } | Select-Object -First 1
                    }
                    else {
                        $asset = $Assets | Where-Object { $_.owner -eq $owner -and $_.role -eq $role -and $_.variant -eq $column.Variant } | Select-Object -First 1
                    }
                    if ($null -eq $asset) { continue }
                    $sourcePath = if ($column.Variant -eq 401) { Resolve-TilesetPath (Get-TilesetProperty $asset "source_path" "") } else { $asset.path }
                    Invoke-WithBitmap $sourcePath {
                        param($bitmap)
                        $dest = [Drawing.Rectangle]::new($x, $yBase + 20 + ($row * $cell), $cell, $cell)
                        $sheet.Graphics.DrawImage($bitmap, $dest, 0, 0, $bitmap.Width, $bitmap.Height, [Drawing.GraphicsUnit]::Pixel)
                    }
                }
            }
            $yBase += 20 + ($rolesInOrder.Count * $cell) + 42
        }
        $sheet.Bitmap.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $sheet.Graphics.Dispose()
        $sheet.Bitmap.Dispose()
    }
}

$waysPath = Join-Path $resolvedSourceRoot "ways.json"
$ways = Get-Content -LiteralPath $waysPath -Raw | ConvertFrom-Json
$workOrders = @(ConvertTo-TilesetArray (Get-TilesetProperty $ways "production_work_orders" @()) | Where-Object {
    (Get-TilesetProperty $_ "asset_class" "") -eq "bridge_sequence" -and
    (Get-TilesetProperty $_ "owner" "") -in @("bridge_wood_simple", "bridge_stone")
})
if ($workOrders.Count -eq 0) {
    throw "No bridge sequence work orders found."
}

$generated = @()
$manifestAssets = @()
foreach ($workOrder in $workOrders) {
    $workOrderId = Get-TilesetProperty $workOrder "id" ""
    $owner = Get-TilesetProperty $workOrder "owner" ""
    $scale = Get-TilesetProperty $workOrder "scale" "world"
    $roles = @(ConvertTo-TilesetArray (Get-TilesetProperty $workOrder "required_masks_or_roles" @()) | ForEach-Object { [string]$_ })
    if ($roles.Count -eq 0) {
        $roles = $rolesInOrder
    }
    $directory = Join-Path $resolvedSourceRoot "bridge_span/$owner"
    New-Item -ItemType Directory -Force -Path $directory | Out-Null

    foreach ($role in $roles) {
        $sourceContext = $ways.assets | Where-Object {
            (Get-TilesetProperty $_ "work_order_id" "") -eq $workOrderId -and
            (Get-TilesetProperty $_ "role" "") -eq $role
        } | Select-Object -First 1

        for ($variant = $variantStart; $variant -le $variantEnd; ++$variant) {
            $variantToken = Format-VariantToken $variant
            $fileName = "fantasy_neutral__$scale`__bridge_span__$owner`__$role`__$variantToken.png"
            $outputPath = Join-Path $directory $fileName
            New-BridgeIntegratedPng $owner $role $variant $outputPath

            $id = "fantasy_neutral.$scale.bridge_span.$owner.$role.$variantToken"
            $context = if ($null -ne $sourceContext) {
                Get-TilesetProperty $sourceContext "context" ([pscustomobject]@{})
            }
            else {
                [pscustomobject]@{
                    bridge_family = $owner
                    span_position = $role
                    orientation = "north_south_sequence_review"
                }
            }
            Set-JsonProperty $context "span_position" $role
            Set-JsonProperty $context "orientation" "north_south_sequence_review"

            $manifest = [pscustomobject]@{
                id = $id
                style = "fantasy_neutral"
                scale = $scale
                asset_class = "bridge_span"
                owner = $owner
                work_order_id = $workOrderId
                role = $role
                span_position = $role
                variant = $variant
                source_path = ConvertTo-TilesetRelativePath $outputPath
                dimensions = [pscustomobject]@{ width = $TileSize; height = $TileSize }
                tile_shape = "square"
                transparent = $false
                edge_contract = "bridge_sequence_v501_bank_integrated_vertical_review"
                tags = @(
                    "runtime_review_candidate",
                    $reviewTag,
                    "bridge_sequence_candidate",
                    "bridge_span",
                    $owner,
                    $role,
                    "candidate_only",
                    "bank_integrated",
                    "approach_way_integrated"
                )
                context = $context
                status = "candidate"
                validation = [pscustomobject]@{
                    human_review = "needs_in_app_user_feedback"
                    notes = "v501 bridge review pass: adds ground approach, irregular bank shape, and abutment/cap detail while preserving bridge span-position metadata."
                }
            }
            $manifestAssets += $manifest
            $generated += [pscustomobject]@{
                id = $id
                owner = $owner
                work_order_id = $workOrderId
                role = $role
                variant = $variant
                path = $outputPath
                source_path = ConvertTo-TilesetRelativePath $outputPath
            }
        }
    }
}

$existingById = @{}
foreach ($asset in @(ConvertTo-TilesetArray (Get-TilesetProperty $ways "assets" @()))) {
    $existingById[(Get-TilesetProperty $asset "id" "")] = $true
}

$newAssets = @()
foreach ($manifest in $manifestAssets) {
    if ($existingById.ContainsKey($manifest.id)) {
        $ways.assets = @($ways.assets | ForEach-Object {
            if ((Get-TilesetProperty $_ "id" "") -eq $manifest.id) { $manifest } else { $_ }
        })
    }
    else {
        $newAssets += $manifest
    }
}
if ($newAssets.Count -gt 0) {
    $ways.assets = @($ways.assets) + $newAssets
}
Write-CleanJsonFile $waysPath $ways

$orderedGenerated = @($generated | Sort-Object owner, role, variant)
$contactPath = Join-Path $resolvedOutputDirectory "bridge_bank_integrated_v501_v503_contact_sheet.png"
$sequencePath = Join-Path $resolvedOutputDirectory "bridge_bank_integrated_v501_v503_sequence_review.png"
$comparisonPath = Join-Path $resolvedOutputDirectory "bridge_v401_vs_v501_sequence_comparison.png"
Draw-ContactSheet $orderedGenerated $contactPath
Draw-SequenceSheet $orderedGenerated $sequencePath "bank-integrated sequence review"
Draw-ComparisonSheet $ways $orderedGenerated $comparisonPath

$edgeChecks = @(Test-RouteEdgeContrast $orderedGenerated)
$reportPath = Resolve-TilesetPath "build/bridge_bank_integrated_v501_v503_sequence_report.json"
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $reportPath) | Out-Null
Write-CleanJsonFile $reportPath ([pscustomobject]@{
    generated_at_local = (Get-Date).ToString("yyyy-MM-ddTHH:mm:ssK")
    tag = $reviewTag
    generated_asset_count = $generated.Count
    work_order_ids = @($workOrders | ForEach-Object { Get-TilesetProperty $_ "id" "" })
    variants = @($variantStart..$variantEnd)
    roles = $rolesInOrder
    route_edge_contrast = [pscustomobject]@{
        checked_edges = $edgeChecks.Count
        review_count = @($edgeChecks | Where-Object status -eq "review").Count
        checks = $edgeChecks
    }
})

$sidecarPath = Join-Path $resolvedOutputDirectory "bridge_bank_integrated_v501_v503_review.json"
Write-CleanJsonFile $sidecarPath ([pscustomobject]@{
    schema_version = 1
    generated_at_local = (Get-Date).ToString("yyyy-MM-ddTHH:mm:ssK")
    status = "candidate_only_needs_human_art_direction_review"
    tag = $reviewTag
    work_order_ids = @($workOrders | ForEach-Object { Get-TilesetProperty $_ "id" "" })
    generated_asset_count = $generated.Count
    evidence = [pscustomobject]@{
        contact_sheet = ConvertTo-TilesetRelativePath $contactPath
        sequence_review_sheet = ConvertTo-TilesetRelativePath $sequencePath
        v401_comparison_sheet = ConvertTo-TilesetRelativePath $comparisonPath
        sequence_report = ConvertTo-TilesetRelativePath $reportPath
    }
    findings = @(
        "v401 bridges are technically coherent but read as straight stamped strips.",
        "v501-v503 add real approach tiles, irregular shore/bank shape, and abutment/cap detail.",
        "The pass remains north-south sequence review only; renderer rotation still needs generated-map review before promotion.",
        "No bridge asset is accepted by this script."
    )
})

Write-Host "Generated bridge bank-integrated candidates: $($generated.Count)"
Write-Host "Contact sheet: $contactPath"
Write-Host "Sequence sheet: $sequencePath"
Write-Host "Comparison sheet: $comparisonPath"
Write-Host "Sequence report: $reportPath"

param(
    [string]$OutputDirectory = "Resources/Maps/Tilesets",
    [string]$StyleReferenceSource = "",
    [string]$SourceDirectory = ""
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
if (-not $SourceDirectory) {
    $SourceDirectory = Join-Path $OutputDirectory "Source/FantasyNeutral"
}

if (-not (Test-Path -LiteralPath $SourceDirectory)) {
    throw "Missing AI source directory: $SourceDirectory"
}

if ($StyleReferenceSource -and (Test-Path -LiteralPath $StyleReferenceSource)) {
    Copy-Item -LiteralPath $StyleReferenceSource -Destination (Join-Path $OutputDirectory "IsometricStyleReference.png") -Force
}
elseif (Test-Path -LiteralPath (Join-Path $SourceDirectory "style_reference_contact_sheet.png")) {
    Copy-Item -LiteralPath (Join-Path $SourceDirectory "style_reference_contact_sheet.png") -Destination (Join-Path $OutputDirectory "IsometricStyleReference.png") -Force
}

$script:SourceCache = @{}

function Get-SourceBitmap([string]$Name) {
    if (-not $script:SourceCache.ContainsKey($Name)) {
        $path = Join-Path $SourceDirectory $Name
        if (-not (Test-Path -LiteralPath $path)) {
            throw "Missing AI source sheet: $path"
        }
        $script:SourceCache[$Name] = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $path).Path)
    }
    return $script:SourceCache[$Name]
}

function Cell([string]$Source, [int]$Column, [int]$Row, [int]$Rotate = 0, [bool]$Tileable = $false) {
    return [pscustomobject]@{
        Source = $Source
        Column = $Column
        Row = $Row
        Rotate = $Rotate
        Tileable = $Tileable
    }
}

function Blend-Channel([int]$A, [int]$B, [double]$Weight) {
    return [int][Math]::Round(($A * (1.0 - $Weight)) + ($B * $Weight))
}

function Blend-Color([Drawing.Color]$A, [Drawing.Color]$B, [double]$Weight) {
    return [Drawing.Color]::FromArgb(
        (Blend-Channel $A.A $B.A $Weight),
        (Blend-Channel $A.R $B.R $Weight),
        (Blend-Channel $A.G $B.G $Weight),
        (Blend-Channel $A.B $B.B $Weight)
    )
}

function Average-Color([Drawing.Color]$A, [Drawing.Color]$B) {
    return [Drawing.Color]::FromArgb(
        [int][Math]::Round(($A.A + $B.A) / 2.0),
        [int][Math]::Round(($A.R + $B.R) / 2.0),
        [int][Math]::Round(($A.G + $B.G) / 2.0),
        [int][Math]::Round(($A.B + $B.B) / 2.0)
    )
}

function Positive-Mod([int]$Value, [int]$Modulo) {
    $result = $Value % $Modulo
    if ($result -lt 0) {
        $result += $Modulo
    }
    return $result
}

function Normalize-TileEdges([Drawing.Bitmap]$Tile) {
    # AI-generated auto-tile source cells are curated for soft repeatable edges.
    # Keep this hook in place for future post-processing without adding slow pixel loops.
    return
}

function Draw-SourceCell($Graphics, $Spec, [int]$DestColumn, [int]$DestRow) {
    $source = Get-SourceBitmap $Spec.Source
    $cellWidth = [double]$source.Width / 8.0
    $cellHeight = [double]$source.Height / 8.0
    $marginX = [Math]::Max(3, [int][Math]::Round($cellWidth * 0.035))
    $marginY = [Math]::Max(3, [int][Math]::Round($cellHeight * 0.035))

    $left = [int][Math]::Round($Spec.Column * $cellWidth) + $marginX
    $top = [int][Math]::Round($Spec.Row * $cellHeight) + $marginY
    $right = [int][Math]::Round(($Spec.Column + 1) * $cellWidth) - $marginX
    $bottom = [int][Math]::Round(($Spec.Row + 1) * $cellHeight) - $marginY
    $sourceRect = [Drawing.Rectangle]::new($left, $top, [Math]::Max(1, $right - $left), [Math]::Max(1, $bottom - $top))
    $destRect = [Drawing.Rectangle]::new($DestColumn * 128, $DestRow * 128, 128, 128)

    if ($Spec.Rotate -eq 0 -and -not $Spec.Tileable) {
        $Graphics.DrawImage($source, $destRect, $sourceRect, [Drawing.GraphicsUnit]::Pixel)
        return
    }

    $tile = [Drawing.Bitmap]::new(128, 128, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $tileGraphics = [Drawing.Graphics]::FromImage($tile)
    $tileGraphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $tileGraphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $tileGraphics.DrawImage($source, [Drawing.Rectangle]::new(0, 0, 128, 128), $sourceRect, [Drawing.GraphicsUnit]::Pixel)
    $tileGraphics.Dispose()

    if ($Spec.Tileable) {
        Normalize-TileEdges $tile
    }

    switch (($Spec.Rotate % 360 + 360) % 360) {
        90 { $tile.RotateFlip([Drawing.RotateFlipType]::Rotate90FlipNone) }
        180 { $tile.RotateFlip([Drawing.RotateFlipType]::Rotate180FlipNone) }
        270 { $tile.RotateFlip([Drawing.RotateFlipType]::Rotate270FlipNone) }
    }
    $Graphics.DrawImage($tile, $destRect)
    $tile.Dispose()
}

function Draw-SourceCellAlpha($Graphics, $Spec, [int]$DestColumn, [int]$DestRow, [int]$Alpha, [int]$Inset = 22, [int]$Seed = 0) {
    $source = Get-SourceBitmap $Spec.Source
    $cellWidth = [double]$source.Width / 8.0
    $cellHeight = [double]$source.Height / 8.0
    $marginX = [Math]::Max(3, [int][Math]::Round($cellWidth * 0.035))
    $marginY = [Math]::Max(3, [int][Math]::Round($cellHeight * 0.035))

    $left = [int][Math]::Round($Spec.Column * $cellWidth) + $marginX
    $top = [int][Math]::Round($Spec.Row * $cellHeight) + $marginY
    $right = [int][Math]::Round(($Spec.Column + 1) * $cellWidth) - $marginX
    $bottom = [int][Math]::Round(($Spec.Row + 1) * $cellHeight) - $marginY
    $sourceRect = [Drawing.Rectangle]::new($left, $top, [Math]::Max(1, $right - $left), [Math]::Max(1, $bottom - $top))

    $tile = [Drawing.Bitmap]::new(128, 128, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $tileGraphics = [Drawing.Graphics]::FromImage($tile)
    $tileGraphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $tileGraphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $tileGraphics.DrawImage($source, [Drawing.Rectangle]::new(0, 0, 128, 128), $sourceRect, [Drawing.GraphicsUnit]::Pixel)
    $tileGraphics.Dispose()

    if ($Spec.Tileable) {
        Normalize-TileEdges $tile
    }

    switch (($Spec.Rotate % 360 + 360) % 360) {
        90 { $tile.RotateFlip([Drawing.RotateFlipType]::Rotate90FlipNone) }
        180 { $tile.RotateFlip([Drawing.RotateFlipType]::Rotate180FlipNone) }
        270 { $tile.RotateFlip([Drawing.RotateFlipType]::Rotate270FlipNone) }
    }

    $x = $DestColumn * 128
    $y = $DestRow * 128
    $state = $Graphics.Save()
    $clipPath = [Drawing.Drawing2D.GraphicsPath]::new()
    $clipWidth = [Math]::Max(28, 128 - ($Inset * 2))
    $clipHeight = [Math]::Max(28, 128 - ($Inset * 2))
    $baseSeed = $Seed + ($DestColumn * 37) + ($DestRow * 59) + ($Spec.Column * 71) + ($Spec.Row * 89)
    for ($lobe = 0; $lobe -lt 4; ++$lobe) {
        $rw = [single](42 + (Positive-Mod ($baseSeed + ($lobe * 17)) 34))
        $rh = [single](34 + (Positive-Mod ($baseSeed + ($lobe * 23)) 30))
        $rx = [single]($x + $Inset + (Positive-Mod ($baseSeed + ($lobe * 31)) ([Math]::Max(1, $clipWidth - [int]$rw))))
        $ry = [single]($y + $Inset + (Positive-Mod ($baseSeed + ($lobe * 43)) ([Math]::Max(1, $clipHeight - [int]$rh))))
        $clipPath.AddEllipse([Drawing.RectangleF]::new($rx, $ry, $rw, $rh))
    }
    $clipPath.AddEllipse([Drawing.RectangleF]::new(
            [single]($x + $Inset + 10),
            [single]($y + $Inset + 8),
            [single]([Math]::Max(24, $clipWidth - 20)),
            [single]([Math]::Max(24, $clipHeight - 16))
        ))
    $Graphics.SetClip($clipPath, [Drawing.Drawing2D.CombineMode]::Replace)

    $matrix = [Drawing.Imaging.ColorMatrix]::new()
    $matrix.Matrix33 = [Math]::Max(0.0, [Math]::Min(1.0, $Alpha / 255.0))
    $attributes = [Drawing.Imaging.ImageAttributes]::new()
    $attributes.SetColorMatrix($matrix, [Drawing.Imaging.ColorMatrixFlag]::Default, [Drawing.Imaging.ColorAdjustType]::Bitmap)
    $Graphics.DrawImage(
        $tile,
        [Drawing.Rectangle]::new($x, $y, 128, 128),
        0,
        0,
        128,
        128,
        [Drawing.GraphicsUnit]::Pixel,
        $attributes
    )
    $attributes.Dispose()
    $Graphics.Restore($state)
    $clipPath.Dispose()
    $tile.Dispose()
}

function New-Atlas([string]$Path, [object[]]$Cells) {
    if ($Cells.Count -ne 64) {
        throw "Atlas $Path needs exactly 64 cell specs, got $($Cells.Count)."
    }

    $bitmap = [Drawing.Bitmap]::new(1024, 1024, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.Clear([Drawing.Color]::Transparent)

    for ($index = 0; $index -lt 64; ++$index) {
        Draw-SourceCell $graphics $Cells[$index] ($index % 8) ([int][Math]::Floor($index / 8))
    }

    $graphics.Dispose()
    $bitmap.Save($Path, [Drawing.Imaging.ImageFormat]::Png)
    $bitmap.Dispose()
}

function SequentialCells([string]$Source) {
    $cells = @()
    for ($row = 0; $row -lt 8; ++$row) {
        for ($column = 0; $column -lt 8; ++$column) {
            $cells += Cell $Source $column $row
        }
    }
    return ,$cells
}

function New-RepeatedAutoAtlas([string]$Path, [object[]]$Variants) {
    $cells = @()
    for ($row = 0; $row -lt 8; ++$row) {
        for ($column = 0; $column -lt 8; ++$column) {
            $cells += $Variants[$row % $Variants.Count]
        }
    }
    New-Atlas $Path $cells
}

function Add-InteriorMaterialVariation($Graphics, [int]$DestColumn, [int]$DestRow, [int]$Variant, [Drawing.Color]$Tint) {
    if ($Variant -eq 0) {
        return
    }

    $x = $DestColumn * 128
    $y = $DestRow * 128
    $state = $Graphics.Save()
    $Graphics.SetClip([Drawing.Rectangle]::new($x + 18, $y + 18, 92, 92))

    $softTint = [Drawing.Color]::FromArgb(10, $Tint.R, $Tint.G, $Tint.B)
    $brush = [Drawing.SolidBrush]::new($softTint)
    $Graphics.FillEllipse($brush, $x + 16 + ($Variant * 11), $y + 20 + ($Variant * 7), 96, 76)
    $Graphics.FillEllipse($brush, $x + 34 - ($Variant * 5), $y + 48, 80, 58)
    $brush.Dispose()

    $lineColor = [Drawing.Color]::FromArgb(12, $Tint.R, $Tint.G, $Tint.B)
    $pen = [Drawing.Pen]::new($lineColor, 2.0)
    $Graphics.DrawBezier($pen,
        [Drawing.Point]::new($x + 24, $y + 42 + $Variant),
        [Drawing.Point]::new($x + 48, $y + 30 - $Variant),
        [Drawing.Point]::new($x + 78, $y + 98 + $Variant),
        [Drawing.Point]::new($x + 104, $y + 74 - $Variant))
    $pen.Dispose()

    $Graphics.Restore($state)
}

function Select-InteriorSpec([object[]]$InteriorSpecs, [int]$Column, [int]$Row, [int]$Variant, [int]$Offset = 0) {
    if ($InteriorSpecs.Count -eq 0) {
        return $null
    }
    $index = Positive-Mod (($Column * 5) + ($Row * 7) + ($Variant * 11) + $Offset) $InteriorSpecs.Count
    return $InteriorSpecs[$index]
}

function New-MaterialAutoAtlas([string]$Path, $BaseSpec, [Drawing.Color[]]$VariantTints, [object[]]$InteriorSpecs = @()) {
    $bitmap = [Drawing.Bitmap]::new(1024, 1024, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.Clear([Drawing.Color]::Transparent)

    for ($row = 0; $row -lt 8; ++$row) {
        for ($column = 0; $column -lt 8; ++$column) {
            $variant = $row % 4
            $mask = $column + ([int][Math]::Floor($row / 4) * 8)
            Draw-SourceCell $graphics $BaseSpec $column $row
            if ($InteriorSpecs.Count -gt 0) {
                $primarySpec = Select-InteriorSpec $InteriorSpecs $column $row $variant $mask
                $primaryAlpha = 24 + (Positive-Mod (($column * 13) + ($row * 17) + ($variant * 19)) 24)
                Draw-SourceCellAlpha $graphics $primarySpec $column $row $primaryAlpha 18 (($mask * 31) + ($variant * 41))

                $secondarySpec = Select-InteriorSpec $InteriorSpecs $column $row $variant (37 + ($mask * 3))
                $secondaryAlpha = 16 + (Positive-Mod (($column * 23) + ($row * 11) + ($variant * 29)) 18)
                Draw-SourceCellAlpha $graphics $secondarySpec $column $row $secondaryAlpha 34 (($mask * 53) + ($variant * 67))
            }
            Add-InteriorMaterialVariation $graphics $column $row $variant $VariantTints[$variant % $VariantTints.Count]
        }
    }

    $graphics.Dispose()
    $bitmap.Save($Path, [Drawing.Imaging.ImageFormat]::Png)
    $bitmap.Dispose()
}

function New-OverlayBrush([Drawing.Color]$Color, [int]$Alpha) {
    return [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb($Alpha, $Color.R, $Color.G, $Color.B))
}

function New-OverlayPen([Drawing.Color]$Color, [float]$Width, [int]$Alpha) {
    $pen = [Drawing.Pen]::new([Drawing.Color]::FromArgb($Alpha, $Color.R, $Color.G, $Color.B), $Width)
    $pen.StartCap = [Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [Drawing.Drawing2D.LineCap]::Round
    $pen.LineJoin = [Drawing.Drawing2D.LineJoin]::Round
    return $pen
}

function Get-TransitionPalette([string]$Name) {
    switch ($Name) {
        "GrassWater" {
            return @{
                Base = [Drawing.Color]::FromArgb(64, 92, 53)
                Accent = [Drawing.Color]::FromArgb(48, 118, 124)
                Detail = [Drawing.Color]::FromArgb(150, 132, 76)
                Alpha = 120
                Style = "Reeds"
            }
        }
        "GrassDesert" {
            return @{
                Base = [Drawing.Color]::FromArgb(190, 156, 88)
                Accent = [Drawing.Color]::FromArgb(118, 132, 58)
                Detail = [Drawing.Color]::FromArgb(222, 190, 116)
                Alpha = 108
                Style = "Sand"
            }
        }
        "GrassSnow" {
            return @{
                Base = [Drawing.Color]::FromArgb(224, 234, 236)
                Accent = [Drawing.Color]::FromArgb(142, 172, 190)
                Detail = [Drawing.Color]::FromArgb(245, 248, 248)
                Alpha = 122
                Style = "Frost"
            }
        }
        "GrassRock" {
            return @{
                Base = [Drawing.Color]::FromArgb(102, 100, 90)
                Accent = [Drawing.Color]::FromArgb(156, 148, 126)
                Detail = [Drawing.Color]::FromArgb(70, 72, 68)
                Alpha = 128
                Style = "Rock"
            }
        }
        "GrassSwamp" {
            return @{
                Base = [Drawing.Color]::FromArgb(56, 72, 46)
                Accent = [Drawing.Color]::FromArgb(66, 92, 74)
                Detail = [Drawing.Color]::FromArgb(128, 122, 62)
                Alpha = 120
                Style = "Reeds"
            }
        }
        "WaterSand" {
            return @{
                Base = [Drawing.Color]::FromArgb(214, 178, 104)
                Accent = [Drawing.Color]::FromArgb(82, 158, 166)
                Detail = [Drawing.Color]::FromArgb(236, 210, 142)
                Alpha = 105
                Style = "Shallows"
            }
        }
        "SnowRock" {
            return @{
                Base = [Drawing.Color]::FromArgb(116, 118, 112)
                Accent = [Drawing.Color]::FromArgb(210, 222, 226)
                Detail = [Drawing.Color]::FromArgb(82, 84, 82)
                Alpha = 116
                Style = "Rock"
            }
        }
        "DungeonFloorWall" {
            return @{
                Base = [Drawing.Color]::FromArgb(36, 32, 28)
                Accent = [Drawing.Color]::FromArgb(98, 86, 72)
                Detail = [Drawing.Color]::FromArgb(18, 16, 14)
                Alpha = 128
                Style = "Shadow"
            }
        }
    }
    throw "Unknown transition palette: $Name"
}

function Get-EdgeRect([int]$X, [int]$Y, [string]$Direction, [int]$Band) {
    $thickness = 4
    switch ($Direction) {
        "N" { return [Drawing.Rectangle]::new($X, $Y + ($Band * $thickness), 128, $thickness) }
        "E" { return [Drawing.Rectangle]::new($X + 128 - (($Band + 1) * $thickness), $Y, $thickness, 128) }
        "S" { return [Drawing.Rectangle]::new($X, $Y + 128 - (($Band + 1) * $thickness), 128, $thickness) }
        "W" { return [Drawing.Rectangle]::new($X + ($Band * $thickness), $Y, $thickness, 128) }
    }
}

function Draw-TransitionDetail($Graphics, [int]$X, [int]$Y, [string]$Direction, $Palette, [int]$Variant, [int]$Index) {
    $offset = (($Index * 17) + ($Variant * 19)) % 104
    $jitter = (($Index * 23) + ($Variant * 7)) % 12
    switch ($Direction) {
        "N" { $px = $X + 12 + $offset; $py = $Y + 4 + $jitter }
        "E" { $px = $X + 112 - $jitter; $py = $Y + 12 + $offset }
        "S" { $px = $X + 12 + $offset; $py = $Y + 112 - $jitter }
        default { $px = $X + 4 + $jitter; $py = $Y + 12 + $offset }
    }

    if ($Palette.Style -eq "Rock") {
        $brush = New-OverlayBrush $Palette.Detail (95 + (($Index + $Variant) % 3) * 18)
        $Graphics.FillEllipse($brush, $px, $py, 6 + (($Index + $Variant) % 5), 4 + (($Index * 2) % 4))
        $brush.Dispose()
        return
    }

    if ($Palette.Style -eq "Reeds") {
        $pen = New-OverlayPen $Palette.Detail 1.4 (105 + (($Index + $Variant) % 3) * 16)
        $Graphics.DrawLine($pen, $px, $py + 9, $px + 2 - ($Index % 4), $py)
        $pen.Dispose()
        return
    }

    if ($Palette.Style -eq "Frost") {
        $pen = New-OverlayPen $Palette.Detail 1.2 90
        $Graphics.DrawLine($pen, $px - 4, $py + 2, $px + 8, $py - 1)
        $pen.Dispose()
        return
    }

    if ($Palette.Style -eq "Shadow") {
        $brush = New-OverlayBrush $Palette.Detail 70
        $Graphics.FillEllipse($brush, $px - 5, $py - 2, 14, 7)
        $brush.Dispose()
        return
    }

    $brush = New-OverlayBrush $Palette.Detail 78
    $Graphics.FillEllipse($brush, $px, $py, 5, 3)
    $brush.Dispose()
}

function Draw-TransitionEdge($Graphics, [int]$CellX, [int]$CellY, [string]$Direction, $Palette, [int]$Variant) {
    $x = $CellX * 128
    $y = $CellY * 128
    for ($band = 0; $band -lt 7; ++$band) {
        $alpha = [Math]::Max(0, [int]($Palette.Alpha - ($band * 17)))
        $bandColor = if ($band -lt 2) { $Palette.Base } else { $Palette.Accent }
        $brush = New-OverlayBrush $bandColor $alpha
        $Graphics.FillRectangle($brush, (Get-EdgeRect $x $y $Direction $band))
        $brush.Dispose()
    }

    $pen = New-OverlayPen $Palette.Accent 2.2 82
    switch ($Direction) {
        "N" { $Graphics.DrawBezier($pen, $x + 5, $y + 19 + $Variant, $x + 38, $y + 11, $x + 88, $y + 27, $x + 123, $y + 16) }
        "E" { $Graphics.DrawBezier($pen, $x + 110 - $Variant, $y + 5, $x + 119, $y + 38, $x + 101, $y + 88, $x + 113, $y + 123) }
        "S" { $Graphics.DrawBezier($pen, $x + 5, $y + 110 - $Variant, $x + 38, $y + 119, $x + 88, $y + 101, $x + 123, $y + 113) }
        "W" { $Graphics.DrawBezier($pen, $x + 18 + $Variant, $y + 5, $x + 9, $y + 38, $x + 27, $y + 88, $x + 15, $y + 123) }
    }
    $pen.Dispose()

    for ($i = 0; $i -lt 9; ++$i) {
        Draw-TransitionDetail $Graphics $x $y $Direction $Palette $Variant $i
    }
}

function Draw-TransitionCorner($Graphics, [int]$CellX, [int]$CellY, [string]$Corner, $Palette) {
    $x = $CellX * 128
    $y = $CellY * 128
    switch ($Corner) {
        "NE" { $rect = [Drawing.Rectangle]::new($x + 96, $y - 8, 40, 40) }
        "SE" { $rect = [Drawing.Rectangle]::new($x + 96, $y + 96, 40, 40) }
        "SW" { $rect = [Drawing.Rectangle]::new($x - 8, $y + 96, 40, 40) }
        default { $rect = [Drawing.Rectangle]::new($x - 8, $y - 8, 40, 40) }
    }
    $brush = New-OverlayBrush $Palette.Base ([int]($Palette.Alpha * 0.78))
    $Graphics.FillEllipse($brush, $rect)
    $brush.Dispose()
}

function New-TransitionOverlayAtlas([string]$Path, [string]$PaletteName) {
    $palette = Get-TransitionPalette $PaletteName
    $bitmap = [Drawing.Bitmap]::new(1024, 1024, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.Clear([Drawing.Color]::Transparent)

    for ($row = 0; $row -lt 8; ++$row) {
        for ($column = 0; $column -lt 8; ++$column) {
            $variant = $row % 4
            $mask = $column + ([int][Math]::Floor($row / 4) * 8)
            if (($mask -band 1) -ne 0) { Draw-TransitionEdge $graphics $column $row "N" $palette $variant }
            if (($mask -band 2) -ne 0) { Draw-TransitionEdge $graphics $column $row "E" $palette $variant }
            if (($mask -band 4) -ne 0) { Draw-TransitionEdge $graphics $column $row "S" $palette $variant }
            if (($mask -band 8) -ne 0) { Draw-TransitionEdge $graphics $column $row "W" $palette $variant }
            if (($mask -band 3) -eq 3) { Draw-TransitionCorner $graphics $column $row "NE" $palette }
            if (($mask -band 6) -eq 6) { Draw-TransitionCorner $graphics $column $row "SE" $palette }
            if (($mask -band 12) -eq 12) { Draw-TransitionCorner $graphics $column $row "SW" $palette }
            if (($mask -band 9) -eq 9) { Draw-TransitionCorner $graphics $column $row "NW" $palette }
        }
    }

    $graphics.Dispose()
    $bitmap.Save($Path, [Drawing.Imaging.ImageFormat]::Png)
    $bitmap.Dispose()
}

function RoadCellForMask([int]$Mask, [int]$Variant) {
    $source = "travel_settlement_source_8x8.png"
    $connections = 0
    foreach ($bit in @(1, 2, 4, 8)) {
        if (($Mask -band $bit) -ne 0) { ++$connections }
    }

    if ($connections -ge 3) { return Cell $source 3 0 }
    if ($Mask -eq 5) { return Cell $source 0 0 }
    if ($Mask -eq 10) { return Cell $source 0 0 90 }
    if ($connections -eq 2) { return Cell $source 2 0 ($Variant * 90) }
    if (($Mask -band 2) -ne 0 -or ($Mask -band 8) -ne 0) { return Cell $source 0 0 90 }
    if (($Mask -band 1) -ne 0 -or ($Mask -band 4) -ne 0) { return Cell $source 0 0 }
    return Cell $source 2 0
}

function New-RoadAutoAtlas([string]$Path) {
    $cells = @()
    for ($row = 0; $row -lt 8; ++$row) {
        for ($column = 0; $column -lt 8; ++$column) {
            $variant = $row % 4
            $mask = $column + ([int][Math]::Floor($row / 4) * 8)
            $cells += RoadCellForMask $mask $variant
        }
    }
    New-Atlas $Path $cells
}

$terrain = "auto_terrain_source_8x8.png"
$nature = "nature_landmarks_source_8x8.png"
$travel = "travel_settlement_source_8x8.png"
$dungeon = "dungeon_source_8x8.png"
$battlefield = "battlefield_source_8x8.png"

New-Atlas (Join-Path $OutputDirectory "IsometricTerrainTiles.png") (SequentialCells $terrain)
New-Atlas (Join-Path $OutputDirectory "IsometricNatureTiles.png") (SequentialCells $nature)
New-Atlas (Join-Path $OutputDirectory "IsometricTravelAndMapTiles.png") (SequentialCells $travel)

$settlementCells = @(
    (Cell $travel 1 2), (Cell $travel 0 2), (Cell $travel 2 2), (Cell $travel 0 5),
    (Cell $travel 3 2), (Cell $travel 4 2), (Cell $travel 6 2), (Cell $travel 7 2),
    (Cell $travel 5 2), (Cell $travel 1 3), (Cell $travel 2 3), (Cell $travel 3 3),
    (Cell $travel 5 3), (Cell $travel 6 3), (Cell $travel 7 3), (Cell $travel 3 5)
)
while ($settlementCells.Count -lt 64) {
    for ($i = 0; $i -lt 64 -and $settlementCells.Count -lt 64; ++$i) {
        $settlementCells += (SequentialCells $travel)[$i]
    }
}
New-Atlas (Join-Path $OutputDirectory "IsometricSettlementTiles.png") $settlementCells

$dungeonCells = SequentialCells $dungeon
$dungeonCells[0] = Cell $dungeon 0 0
$dungeonCells[1] = Cell $dungeon 2 0
$dungeonCells[2] = Cell $dungeon 3 0
$dungeonCells[3] = Cell $dungeon 5 0
$dungeonCells[4] = Cell $dungeon 0 0
$dungeonCells[5] = Cell $dungeon 7 6
$dungeonCells[6] = Cell $dungeon 0 5
$dungeonCells[7] = Cell $dungeon 7 4
$dungeonCells[8] = Cell $dungeon 7 5
$dungeonCells[9] = Cell $dungeon 5 3
$dungeonCells[10] = Cell $dungeon 5 4
New-Atlas (Join-Path $OutputDirectory "IsometricDungeonTiles.png") $dungeonCells

$battlefieldCells = SequentialCells $battlefield
$battlefieldCells[0] = Cell $battlefield 7 2
$battlefieldCells[1] = Cell $battlefield 6 2
$battlefieldCells[2] = Cell $battlefield 3 2
$battlefieldCells[3] = Cell $battlefield 4 0
$battlefieldCells[4] = Cell $battlefield 2 2
$battlefieldCells[5] = Cell $battlefield 7 4
$battlefieldCells[6] = Cell $battlefield 1 3
$battlefieldCells[7] = Cell $battlefield 3 3
New-Atlas (Join-Path $OutputDirectory "IsometricBattlefieldTiles.png") $battlefieldCells

New-Atlas (Join-Path $OutputDirectory "IsometricTokensAndProps.png") (SequentialCells $battlefield)

# Connected material variants share identical borders and only vary softly inside the tile.
# This keeps grass-next-to-grass flowing as one field while avoiding a single stamped repeat.
$grassTints = [Drawing.Color[]]@(
    [Drawing.Color]::FromArgb(0, 122, 145, 70),
    [Drawing.Color]::FromArgb(255, 142, 164, 82),
    [Drawing.Color]::FromArgb(255, 84, 116, 66),
    [Drawing.Color]::FromArgb(255, 166, 154, 92)
)
$grassInteriorSpecs = @(
    (Cell $terrain 0 0 0 $true),
    (Cell $terrain 1 0 0 $true),
    (Cell $terrain 2 0 0 $true),
    (Cell $terrain 3 0 0 $true)
)
$dirtInteriorSpecs = @(
    (Cell $terrain 0 1 0 $true),
    (Cell $terrain 1 1 0 $true),
    (Cell $terrain 2 1 0 $true),
    (Cell $terrain 3 1 0 $true),
    (Cell $terrain 4 1 0 $true),
    (Cell $terrain 5 1 0 $true),
    (Cell $terrain 6 1 0 $true),
    (Cell $terrain 7 1 0 $true)
)
$waterInteriorSpecs = @(
    (Cell $terrain 0 2 0 $true),
    (Cell $terrain 1 2 0 $true),
    (Cell $terrain 2 2 0 $true),
    (Cell $terrain 3 2 0 $true),
    (Cell $terrain 4 2 0 $true),
    (Cell $terrain 6 2 0 $true)
)
$stoneInteriorSpecs = @(
    (Cell $terrain 0 3 0 $true),
    (Cell $terrain 1 3 0 $true),
    (Cell $terrain 2 3 0 $true),
    (Cell $terrain 3 3 0 $true),
    (Cell $terrain 4 3 0 $true),
    (Cell $terrain 5 3 0 $true),
    (Cell $terrain 6 3 0 $true),
    (Cell $terrain 7 3 0 $true)
)
$snowInteriorSpecs = @(
    (Cell $terrain 0 4 0 $true),
    (Cell $terrain 1 4 0 $true),
    (Cell $terrain 2 4 0 $true),
    (Cell $terrain 3 4 0 $true)
)
$desertInteriorSpecs = @(
    (Cell $terrain 0 5 0 $true),
    (Cell $terrain 1 5 0 $true),
    (Cell $terrain 2 5 0 $true),
    (Cell $terrain 3 5 0 $true),
    (Cell $terrain 4 5 0 $true),
    (Cell $terrain 5 5 0 $true),
    (Cell $terrain 7 5 0 $true)
)
$swampInteriorSpecs = @(
    (Cell $terrain 0 6 0 $true),
    (Cell $terrain 1 6 0 $true),
    (Cell $terrain 2 6 0 $true),
    (Cell $terrain 3 6 0 $true),
    (Cell $terrain 4 6 0 $true),
    (Cell $terrain 5 6 0 $true),
    (Cell $terrain 6 6 0 $true),
    (Cell $terrain 7 6 0 $true)
)
$iceInteriorSpecs = @(
    (Cell $terrain 4 4 0 $true),
    (Cell $terrain 5 4 0 $true),
    (Cell $terrain 6 4 0 $true),
    (Cell $terrain 7 4 0 $true)
)
$dungeonFloorInteriorSpecs = @(
    (Cell $dungeon 0 0 0 $true),
    (Cell $dungeon 1 0 0 $true),
    (Cell $dungeon 0 1 0 $true),
    (Cell $dungeon 1 1 0 $true),
    (Cell $dungeon 2 1 0 $true),
    (Cell $dungeon 4 1 0 $true)
)
$dungeonWallInteriorSpecs = @(
    (Cell $dungeon 2 0),
    (Cell $dungeon 0 2),
    (Cell $dungeon 1 2),
    (Cell $dungeon 2 2),
    (Cell $dungeon 3 2),
    (Cell $dungeon 6 2)
)
$hazardInteriorSpecs = @(
    (Cell $battlefield 0 4),
    (Cell $battlefield 3 3),
    (Cell $battlefield 4 2),
    (Cell $battlefield 7 3),
    (Cell $dungeon 0 4),
    (Cell $dungeon 5 4),
    (Cell $dungeon 6 4)
)
$lavaInteriorSpecs = @(
    (Cell $terrain 0 7 0 $true),
    (Cell $terrain 1 7 0 $true),
    (Cell $terrain 2 7 0 $true),
    (Cell $dungeon 0 6 0 $true),
    (Cell $dungeon 7 5 0 $true)
)
$pitInteriorSpecs = @(
    (Cell $battlefield 1 3),
    (Cell $battlefield 2 3),
    (Cell $dungeon 0 5),
    (Cell $dungeon 1 5),
    (Cell $dungeon 3 5),
    (Cell $dungeon 4 5)
)
$chasmInteriorSpecs = @(
    (Cell $battlefield 0 2),
    (Cell $battlefield 1 2),
    (Cell $battlefield 2 2),
    (Cell $dungeon 1 5),
    (Cell $dungeon 2 5),
    (Cell $dungeon 6 5),
    (Cell $dungeon 7 5)
)
New-MaterialAutoAtlas -Path (Join-Path $OutputDirectory "IsometricAutoGrassTiles.png") -BaseSpec (Cell $terrain 0 0 0 $true) -VariantTints $grassTints -InteriorSpecs $grassInteriorSpecs
New-MaterialAutoAtlas (Join-Path $OutputDirectory "IsometricAutoDirtTiles.png") (Cell $terrain 2 1 0 $true) @(
    [Drawing.Color]::FromArgb(0, 130, 104, 72),
    [Drawing.Color]::FromArgb(255, 148, 118, 82),
    [Drawing.Color]::FromArgb(255, 100, 78, 56),
    [Drawing.Color]::FromArgb(255, 172, 132, 86)
) $dirtInteriorSpecs
New-MaterialAutoAtlas (Join-Path $OutputDirectory "IsometricAutoWaterTiles.png") (Cell $terrain 0 2 0 $true) @(
    [Drawing.Color]::FromArgb(0, 52, 132, 142),
    [Drawing.Color]::FromArgb(255, 72, 168, 174),
    [Drawing.Color]::FromArgb(255, 30, 92, 114),
    [Drawing.Color]::FromArgb(255, 94, 150, 156)
) $waterInteriorSpecs
New-RoadAutoAtlas (Join-Path $OutputDirectory "IsometricAutoRoadTiles.png")
New-MaterialAutoAtlas (Join-Path $OutputDirectory "IsometricAutoStoneTiles.png") (Cell $terrain 1 3 0 $true) @(
    [Drawing.Color]::FromArgb(0, 120, 120, 116),
    [Drawing.Color]::FromArgb(255, 154, 150, 138),
    [Drawing.Color]::FromArgb(255, 88, 92, 90),
    [Drawing.Color]::FromArgb(255, 132, 124, 104)
) $stoneInteriorSpecs
New-MaterialAutoAtlas (Join-Path $OutputDirectory "IsometricAutoSnowTiles.png") (Cell $terrain 2 4 0 $true) @(
    [Drawing.Color]::FromArgb(0, 210, 222, 232),
    [Drawing.Color]::FromArgb(255, 236, 242, 246),
    [Drawing.Color]::FromArgb(255, 166, 192, 212),
    [Drawing.Color]::FromArgb(255, 212, 224, 232)
) $snowInteriorSpecs
New-MaterialAutoAtlas (Join-Path $OutputDirectory "IsometricAutoDesertTiles.png") (Cell $terrain 0 5 0 $true) @(
    [Drawing.Color]::FromArgb(0, 190, 156, 92),
    [Drawing.Color]::FromArgb(255, 216, 176, 104),
    [Drawing.Color]::FromArgb(255, 146, 112, 72),
    [Drawing.Color]::FromArgb(255, 226, 196, 128)
) $desertInteriorSpecs
New-MaterialAutoAtlas (Join-Path $OutputDirectory "IsometricAutoSwampTiles.png") (Cell $terrain 0 6 0 $true) @(
    [Drawing.Color]::FromArgb(0, 74, 92, 50),
    [Drawing.Color]::FromArgb(255, 92, 112, 62),
    [Drawing.Color]::FromArgb(255, 42, 62, 48),
    [Drawing.Color]::FromArgb(255, 126, 116, 62)
) $swampInteriorSpecs
New-MaterialAutoAtlas (Join-Path $OutputDirectory "IsometricAutoIceTiles.png") (Cell $terrain 5 4 0 $true) @(
    [Drawing.Color]::FromArgb(0, 164, 204, 226),
    [Drawing.Color]::FromArgb(255, 190, 224, 238),
    [Drawing.Color]::FromArgb(255, 104, 160, 196),
    [Drawing.Color]::FromArgb(255, 220, 238, 244)
) $iceInteriorSpecs
New-MaterialAutoAtlas (Join-Path $OutputDirectory "IsometricAutoDungeonFloorTiles.png") (Cell $dungeon 0 0 0 $true) @(
    [Drawing.Color]::FromArgb(0, 110, 106, 98),
    [Drawing.Color]::FromArgb(255, 134, 126, 112),
    [Drawing.Color]::FromArgb(255, 72, 70, 68),
    [Drawing.Color]::FromArgb(255, 124, 112, 94)
) $dungeonFloorInteriorSpecs
New-MaterialAutoAtlas (Join-Path $OutputDirectory "IsometricAutoDungeonWallTiles.png") (Cell $dungeon 2 0) @(
    [Drawing.Color]::FromArgb(0, 86, 82, 74),
    [Drawing.Color]::FromArgb(255, 118, 108, 92),
    [Drawing.Color]::FromArgb(255, 48, 46, 42),
    [Drawing.Color]::FromArgb(255, 140, 128, 104)
) $dungeonWallInteriorSpecs
New-MaterialAutoAtlas (Join-Path $OutputDirectory "IsometricAutoHazardTiles.png") (Cell $battlefield 0 4) @(
    [Drawing.Color]::FromArgb(0, 112, 82, 58),
    [Drawing.Color]::FromArgb(255, 190, 128, 72),
    [Drawing.Color]::FromArgb(255, 78, 52, 46),
    [Drawing.Color]::FromArgb(255, 164, 86, 96)
) $hazardInteriorSpecs
New-MaterialAutoAtlas (Join-Path $OutputDirectory "IsometricAutoLavaTiles.png") (Cell $terrain 0 7 0 $true) @(
    [Drawing.Color]::FromArgb(0, 190, 54, 22),
    [Drawing.Color]::FromArgb(255, 255, 118, 26),
    [Drawing.Color]::FromArgb(255, 84, 36, 28),
    [Drawing.Color]::FromArgb(255, 250, 170, 54)
) $lavaInteriorSpecs
New-MaterialAutoAtlas (Join-Path $OutputDirectory "IsometricAutoPitTiles.png") (Cell $battlefield 1 3) @(
    [Drawing.Color]::FromArgb(0, 54, 48, 42),
    [Drawing.Color]::FromArgb(255, 102, 86, 66),
    [Drawing.Color]::FromArgb(255, 22, 22, 22),
    [Drawing.Color]::FromArgb(255, 132, 116, 92)
) $pitInteriorSpecs
New-MaterialAutoAtlas (Join-Path $OutputDirectory "IsometricAutoChasmTiles.png") (Cell $dungeon 1 5) @(
    [Drawing.Color]::FromArgb(0, 42, 42, 40),
    [Drawing.Color]::FromArgb(255, 86, 82, 72),
    [Drawing.Color]::FromArgb(255, 12, 16, 18),
    [Drawing.Color]::FromArgb(255, 116, 106, 88)
) $chasmInteriorSpecs

New-TransitionOverlayAtlas (Join-Path $OutputDirectory "IsometricTransitionGrassWaterTiles.png") "GrassWater"
New-TransitionOverlayAtlas (Join-Path $OutputDirectory "IsometricTransitionGrassDesertTiles.png") "GrassDesert"
New-TransitionOverlayAtlas (Join-Path $OutputDirectory "IsometricTransitionGrassSnowTiles.png") "GrassSnow"
New-TransitionOverlayAtlas (Join-Path $OutputDirectory "IsometricTransitionGrassRockTiles.png") "GrassRock"
New-TransitionOverlayAtlas (Join-Path $OutputDirectory "IsometricTransitionGrassSwampTiles.png") "GrassSwamp"
New-TransitionOverlayAtlas (Join-Path $OutputDirectory "IsometricTransitionWaterSandTiles.png") "WaterSand"
New-TransitionOverlayAtlas (Join-Path $OutputDirectory "IsometricTransitionSnowRockTiles.png") "SnowRock"
New-TransitionOverlayAtlas (Join-Path $OutputDirectory "IsometricTransitionDungeonFloorWallTiles.png") "DungeonFloorWall"

foreach ($bitmap in $script:SourceCache.Values) {
    $bitmap.Dispose()
}

Write-Host "Assembled AI-painted fantasy/neutral tilesets in $OutputDirectory"

param(
    [string]$TilesetDirectory = "Resources/Maps/Tilesets",
    [string]$OutputPath = "Resources/Maps/Tilesets/ConnectedMapPreview.png",
    [int]$TileSize = 40
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$width = 28
$height = 18
$map = @()
for ($y = 0; $y -lt $height; ++$y) {
    $row = @()
    for ($x = 0; $x -lt $width; ++$x) {
        $row += "Empty"
    }
    $map += ,$row
}

function Set-Tile([int]$X, [int]$Y, [string]$Kind) {
    if ($X -ge 0 -and $X -lt $width -and $Y -ge 0 -and $Y -lt $height) {
        $map[$Y][$X] = $Kind
    }
}

function Fill-Rect([int]$X, [int]$Y, [int]$W, [int]$H, [string]$Kind) {
    for ($yy = $Y; $yy -lt $Y + $H; ++$yy) {
        for ($xx = $X; $xx -lt $X + $W; ++$xx) {
            Set-Tile $xx $yy $Kind
        }
    }
}

function Fill-Disc([int]$CenterX, [int]$CenterY, [int]$Radius, [string]$Kind) {
    for ($yy = $CenterY - $Radius; $yy -le $CenterY + $Radius; ++$yy) {
        for ($xx = $CenterX - $Radius; $xx -le $CenterX + $Radius; ++$xx) {
            $dx = $xx - $CenterX
            $dy = $yy - $CenterY
            if (($dx * $dx + $dy * $dy) -le ($Radius * $Radius)) {
                Set-Tile $xx $yy $Kind
            }
        }
    }
}

Fill-Disc 5 4 4 "Water"
Fill-Disc 4 4 2 "Water"
Fill-Disc 8 13 3 "Swamp"
Fill-Rect 0 14 6 4 "Dirt"
Fill-Rect 1 15 4 2 "Desert"
Fill-Rect 22 0 6 3 "Snow"
Fill-Rect 24 1 3 1 "Ice"
Fill-Rect 18 9 8 6 "Room"
Fill-Rect 18 9 8 1 "Wall"
Fill-Rect 18 14 8 1 "Wall"
Fill-Rect 18 9 1 6 "Wall"
Fill-Rect 25 9 1 6 "Wall"
Set-Tile 21 9 "Door"
Set-Tile 22 12 "Pit"
Set-Tile 23 12 "Lava"
Set-Tile 20 11 "Hazard"
Set-Tile 17 11 "Stone"
Set-Tile 16 11 "Bridge"
Set-Tile 15 11 "Water"
Set-Tile 14 11 "Water"
Set-Tile 13 11 "Water"

$road = @(
    @(0, 8), @(1, 8), @(2, 8), @(3, 8), @(4, 8), @(5, 8), @(6, 8), @(7, 8), @(8, 8),
    @(9, 8), @(10, 8), @(11, 8), @(12, 8), @(13, 8), @(14, 8), @(15, 8), @(16, 8),
    @(17, 8), @(18, 8), @(19, 8), @(20, 8), @(21, 8), @(21, 9)
)
foreach ($p in $road) { Set-Tile $p[0] $p[1] "Road" }

foreach ($p in @(@(12, 3), @(13, 3), @(14, 4), @(15, 5), @(16, 6), @(17, 7), @(18, 8))) {
    Set-Tile $p[0] $p[1] "Road"
}

foreach ($p in @(@(10, 5), @(11, 5), @(12, 6), @(13, 6), @(14, 7))) {
    Set-Tile $p[0] $p[1] "Forest"
}

foreach ($p in @(@(3, 11), @(4, 12), @(5, 12), @(7, 2), @(12, 1), @(24, 6))) {
    Set-Tile $p[0] $p[1] "Forest"
}
Set-Tile 11 2 "Mountain"
Set-Tile 12 2 "Hill"
Set-Tile 13 2 "Stone"
Set-Tile 19 2 "Mountain"
Set-Tile 20 2 "Hill"
Set-Tile 21 2 "Stone"
Set-Tile 20 6 "Settlement"
Set-Tile 21 6 "Building"
Set-Tile 22 6 "Cover"
Set-Tile 23 6 "Rubble"

$sheetCache = @{}
function Get-Sheet([string]$Name) {
    if (-not $sheetCache.ContainsKey($Name)) {
        $path = Join-Path $TilesetDirectory $Name
        $sheetCache[$Name] = [Drawing.Bitmap]::new((Resolve-Path $path).Path)
    }
    return $sheetCache[$Name]
}

function Get-Variant([int]$X, [int]$Y, [int]$Salt) {
    $value = ($X * 73856093) -bxor ($Y * 19349663) -bxor ($Salt * 83492791)
    return (($value -band 0x7fffffff) % 4)
}

function Get-Index([int]$X, [int]$Y, [int]$Salt, [int]$Count) {
    if ($Count -le 1) { return 0 }
    $value = ($X * 73856093) -bxor ($Y * 19349663) -bxor ($Salt * 83492791)
    return (($value -band 0x7fffffff) % $Count)
}

function Test-KindConnection([string]$OwnerKind, [string]$NeighborKind) {
    if ($OwnerKind -eq "Road" -or $OwnerKind -eq "Bridge") {
        return $NeighborKind -eq "Road" -or $NeighborKind -eq "Bridge"
    }
    return $NeighborKind -eq $OwnerKind
}

function Get-SameKindMask([int]$X, [int]$Y, [string]$Kind) {
    $mask = 0
    if ($Y -gt 0 -and (Test-KindConnection $Kind $map[$Y - 1][$X])) { $mask = $mask -bor 1 }
    if ($X -lt $width - 1 -and (Test-KindConnection $Kind $map[$Y][$X + 1])) { $mask = $mask -bor 2 }
    if ($Y -lt $height - 1 -and (Test-KindConnection $Kind $map[$Y + 1][$X])) { $mask = $mask -bor 4 }
    if ($X -gt 0 -and (Test-KindConnection $Kind $map[$Y][$X - 1])) { $mask = $mask -bor 8 }
    return $mask
}

function Test-RelationMember([string]$Kind, [string]$Group) {
    switch ($Group) {
        "Water" { return $Kind -eq "Water" }
        "Sand" { return $Kind -eq "Desert" -or $Kind -eq "Dirt" }
        "Snow" { return $Kind -eq "Snow" -or $Kind -eq "Ice" }
        "Rock" { return $Kind -eq "Mountain" -or $Kind -eq "Hill" -or $Kind -eq "Stone" -or $Kind -eq "Rubble" -or $Kind -eq "Cave" -or $Kind -eq "Chasm" }
        "Swamp" { return $Kind -eq "Swamp" }
        "DungeonWall" { return $Kind -eq "Wall" -or $Kind -eq "Door" }
    }
    return $false
}

function Get-RelationMask([int]$X, [int]$Y, [string]$Group) {
    $mask = 0
    if ($Y -gt 0 -and (Test-RelationMember $map[$Y - 1][$X] $Group)) { $mask = $mask -bor 1 }
    if ($X -lt $width - 1 -and (Test-RelationMember $map[$Y][$X + 1] $Group)) { $mask = $mask -bor 2 }
    if ($Y -lt $height - 1 -and (Test-RelationMember $map[$Y + 1][$X] $Group)) { $mask = $mask -bor 4 }
    if ($X -gt 0 -and (Test-RelationMember $map[$Y][$X - 1] $Group)) { $mask = $mask -bor 8 }
    return $mask
}

function New-AutoLayer([string]$Sheet, [int]$Mask, [int]$Variant) {
    return @{ Sheet = $Sheet; A = $Mask; B = $Variant; Auto = $true }
}

function New-CellLayer([string]$Sheet, [int]$Column, [int]$Row) {
    return @{ Sheet = $Sheet; A = $Column; B = $Row; Auto = $false }
}

function New-VariantCellLayer([string]$Sheet, [object[]]$Cells, [int]$X, [int]$Y, [int]$Salt) {
    $cell = $Cells[(Get-Index $X $Y $Salt $Cells.Count)]
    return (New-CellLayer $Sheet $cell[0] $cell[1])
}

function Get-CardinalConnectionCount([int]$Mask) {
    $count = 0
    if (($Mask -band 1) -ne 0) { $count++ }
    if (($Mask -band 2) -ne 0) { $count++ }
    if (($Mask -band 4) -ne 0) { $count++ }
    if (($Mask -band 8) -ne 0) { $count++ }
    return $count
}

function Get-ContextualSymbolShapeVariant([int]$Mask, [int]$Count) {
    if ($Count -le 1) { return 0 }
    if ($Mask -eq 0) { return 0 }
    if ($Count -eq 2) {
        if ((Get-CardinalConnectionCount $Mask) -ge 2) { return 1 }
        return 0
    }

    $horizontal = (($Mask -band (2 -bor 8)) -ne 0)
    $vertical = (($Mask -band (1 -bor 4)) -ne 0)
    if ($horizontal -and -not $vertical) { return [Math]::Min(1, $Count - 1) }
    if ($vertical -and -not $horizontal) { return [Math]::Min(2, $Count - 1) }
    if ((Get-CardinalConnectionCount $Mask) -ge 3) { return [Math]::Min(3, $Count - 1) }
    return [Math]::Min(1, $Count - 1)
}

function New-ContextualSymbolCellLayer([string]$Sheet, [object[]]$Cells, [int]$X, [int]$Y, [string]$Kind, [int]$Salt) {
    $mask = Get-SameKindMask $X $Y $Kind
    $index = if ($mask -ne 0) { Get-ContextualSymbolShapeVariant $mask $Cells.Count } else { Get-Index $X $Y $Salt $Cells.Count }
    $cell = $Cells[$index]
    return (New-CellLayer $Sheet $cell[0] $cell[1])
}

function Resolve-Tile([string]$Kind, [int]$X, [int]$Y) {
    switch ($Kind) {
        "Empty" { return (New-AutoLayer "IsometricAutoGrassTiles.png" (Get-SameKindMask $X $Y $Kind) (Get-Variant $X $Y 1)) }
        "Dirt" { return (New-AutoLayer "IsometricAutoDirtTiles.png" (Get-SameKindMask $X $Y $Kind) (Get-Variant $X $Y 7)) }
        "Water" { return (New-AutoLayer "IsometricAutoWaterTiles.png" (Get-SameKindMask $X $Y $Kind) (Get-Variant $X $Y 2)) }
        "Road" { return (New-AutoLayer "IsometricAutoRoadTiles.png" (Get-SameKindMask $X $Y $Kind) (Get-Variant $X $Y 3)) }
        "Stone" { return (New-AutoLayer "IsometricAutoStoneTiles.png" (Get-SameKindMask $X $Y $Kind) (Get-Variant $X $Y 8)) }
        "Snow" { return (New-AutoLayer "IsometricAutoSnowTiles.png" (Get-SameKindMask $X $Y $Kind) (Get-Variant $X $Y 9)) }
        "Desert" { return (New-AutoLayer "IsometricAutoDesertTiles.png" (Get-SameKindMask $X $Y $Kind) (Get-Variant $X $Y 10)) }
        "Swamp" { return (New-AutoLayer "IsometricAutoSwampTiles.png" (Get-SameKindMask $X $Y $Kind) (Get-Variant $X $Y 11)) }
        "Ice" { return (New-AutoLayer "IsometricAutoIceTiles.png" (Get-SameKindMask $X $Y $Kind) (Get-Variant $X $Y 12)) }
        "Room" { return (New-AutoLayer "IsometricAutoDungeonFloorTiles.png" (Get-SameKindMask $X $Y $Kind) (Get-Variant $X $Y 4)) }
        "Wall" { return (New-AutoLayer "IsometricAutoDungeonWallTiles.png" (Get-SameKindMask $X $Y $Kind) (Get-Variant $X $Y 5)) }
        "Hazard" { return (New-AutoLayer "IsometricAutoHazardTiles.png" (Get-SameKindMask $X $Y $Kind) (Get-Variant $X $Y 6)) }
        "Lava" { return (New-AutoLayer "IsometricAutoLavaTiles.png" (Get-SameKindMask $X $Y $Kind) (Get-Variant $X $Y 13)) }
        "Pit" { return (New-AutoLayer "IsometricAutoPitTiles.png" (Get-SameKindMask $X $Y $Kind) (Get-Variant $X $Y 14)) }
        "Chasm" { return (New-AutoLayer "IsometricAutoChasmTiles.png" (Get-SameKindMask $X $Y $Kind) (Get-Variant $X $Y 15)) }
        "Forest" { return (New-ContextualSymbolCellLayer "IsometricNatureTiles.png" @(@(0, 0), @(1, 0)) $X $Y $Kind 21) }
        "Mountain" { return (New-ContextualSymbolCellLayer "IsometricNatureTiles.png" @(@(0, 1), @(1, 1)) $X $Y $Kind 22) }
        "Hill" { return (New-ContextualSymbolCellLayer "IsometricNatureTiles.png" @(@(6, 0), @(7, 0)) $X $Y $Kind 23) }
        "Settlement" { return (New-VariantCellLayer "IsometricSettlementTiles.png" @(@(0, 0), @(1, 0), @(2, 0), @(3, 0), @(4, 0), @(5, 0), @(6, 0)) $X $Y 24) }
        "Building" { return (New-VariantCellLayer "IsometricSettlementTiles.png" @(@(0, 0), @(1, 0), @(2, 0), @(3, 0)) $X $Y 25) }
        "Door" { return (New-CellLayer "IsometricDungeonTiles.png" 2 0) }
        "Cover" { return (New-VariantCellLayer "IsometricBattlefieldTiles.png" @(@(1, 0), @(5, 2), @(4, 4), @(5, 4), @(7, 4)) $X $Y 29) }
        "Rubble" { return (New-VariantCellLayer "IsometricBattlefieldTiles.png" @(@(0, 0), @(7, 2)) $X $Y 28) }
        "Bridge" { return (New-CellLayer "IsometricBattlefieldTiles.png" 2 0) }
    }
    return (New-CellLayer "IsometricTerrainTiles.png" 0 0)
}

function Get-TransitionLayers([string]$Kind, [int]$X, [int]$Y) {
    $rules = @(
        @{ Owner = "Empty"; Group = "Water"; Sheet = "IsometricTransitionGrassWaterTiles.png"; Salt = 101 },
        @{ Owner = "Empty"; Group = "Sand"; Sheet = "IsometricTransitionGrassDesertTiles.png"; Salt = 102 },
        @{ Owner = "Empty"; Group = "Snow"; Sheet = "IsometricTransitionGrassSnowTiles.png"; Salt = 103 },
        @{ Owner = "Empty"; Group = "Rock"; Sheet = "IsometricTransitionGrassRockTiles.png"; Salt = 104 },
        @{ Owner = "Empty"; Group = "Swamp"; Sheet = "IsometricTransitionGrassSwampTiles.png"; Salt = 105 },
        @{ Owner = "Water"; Group = "Sand"; Sheet = "IsometricTransitionWaterSandTiles.png"; Salt = 106 },
        @{ Owner = "Snow"; Group = "Rock"; Sheet = "IsometricTransitionSnowRockTiles.png"; Salt = 107 },
        @{ Owner = "Room"; Group = "DungeonWall"; Sheet = "IsometricTransitionDungeonFloorWallTiles.png"; Salt = 108 }
    )

    $layers = @()
    foreach ($rule in $rules) {
        if ($Kind -ne $rule.Owner) { continue }
        $mask = Get-RelationMask $X $Y $rule.Group
        if ($mask -eq 0) { continue }
        $layers += (New-AutoLayer $rule.Sheet $mask (Get-Variant $X $Y $rule.Salt))
    }
    return ,$layers
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutputPath) | Out-Null
$bitmap = [Drawing.Bitmap]::new($width * $TileSize, $height * $TileSize, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [Drawing.Graphics]::FromImage($bitmap)
$graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
$graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$graphics.Clear([Drawing.ColorTranslator]::FromHtml("#101418"))

for ($y = 0; $y -lt $height; ++$y) {
    for ($x = 0; $x -lt $width; ++$x) {
        $dest = [Drawing.Rectangle]::new($x * $TileSize, $y * $TileSize, $TileSize, $TileSize)
        $layers = @((Resolve-Tile $map[$y][$x] $x $y)) + (Get-TransitionLayers $map[$y][$x] $x $y)
        foreach ($resolved in $layers) {
            $sheet = Get-Sheet $resolved.Sheet
            if ($resolved.Auto) {
                $mask = [int]$resolved.A
                $variant = [int]$resolved.B
                $sourceX = ($mask % 8) * 128
                $sourceY = ([int][Math]::Floor($mask / 8) * 4 + $variant) * 128
            }
            else {
                $sourceX = [int]$resolved.A * 128
                $sourceY = [int]$resolved.B * 128
            }
            $graphics.DrawImage($sheet, $dest, $sourceX, $sourceY, 128, 128, [Drawing.GraphicsUnit]::Pixel)
        }
    }
}

$graphics.Dispose()
$bitmap.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
$bitmap.Dispose()
foreach ($sheet in $sheetCache.Values) { $sheet.Dispose() }

Write-Host "Generated connected map preview at $OutputPath"

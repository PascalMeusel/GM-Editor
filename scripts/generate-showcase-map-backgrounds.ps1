param(
    [string]$TilesetDirectory = "Resources/Maps/Tilesets",
    [string]$BlackwaterRoot = "Resources/Campaigns/campaign_bell_beneath_blackwater",
    [string]$ClockworkRoot = "Resources/Campaigns/campaign_clockwork_bloodline"
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$script:SheetCache = @{}

function Get-Sheet([string]$Name) {
    if (-not $script:SheetCache.ContainsKey($Name)) {
        $path = Join-Path $TilesetDirectory $Name
        if (-not (Test-Path -LiteralPath $path)) {
            throw "Missing tileset atlas: $path"
        }
        $script:SheetCache[$Name] = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $path).Path)
    }
    return $script:SheetCache[$Name]
}

function AtlasCell([string]$Sheet, [int]$Column, [int]$Row) {
    return [pscustomobject]@{ Sheet = $Sheet; Column = $Column; Row = $Row }
}

$script:AutoSheets = @{
    Grass = "IsometricAutoGrassTiles.png"
    Dirt = "IsometricAutoDirtTiles.png"
    Water = "IsometricAutoWaterTiles.png"
    Road = "IsometricAutoRoadTiles.png"
    Stone = "IsometricAutoStoneTiles.png"
    Snow = "IsometricAutoSnowTiles.png"
    Desert = "IsometricAutoDesertTiles.png"
    Swamp = "IsometricAutoSwampTiles.png"
    Ice = "IsometricAutoIceTiles.png"
    Floor = "IsometricAutoDungeonFloorTiles.png"
    Wall = "IsometricAutoDungeonWallTiles.png"
    Lava = "IsometricAutoLavaTiles.png"
    Pit = "IsometricAutoPitTiles.png"
    Chasm = "IsometricAutoChasmTiles.png"
    Hazard = "IsometricAutoHazardTiles.png"
}

$script:AutoSalts = @{
    Grass = 1
    Dirt = 7
    Water = 2
    Road = 3
    Stone = 8
    Snow = 9
    Desert = 10
    Swamp = 11
    Ice = 12
    Floor = 4
    Wall = 5
    Lava = 13
    Pit = 14
    Chasm = 15
    Hazard = 6
}

$script:TransitionRules = @(
    @{ Owner = "Grass"; Group = "Water"; Sheet = "IsometricTransitionGrassWaterTiles.png"; Salt = 101 },
    @{ Owner = "Grass"; Group = "Sand"; Sheet = "IsometricTransitionGrassDesertTiles.png"; Salt = 102 },
    @{ Owner = "Grass"; Group = "Snow"; Sheet = "IsometricTransitionGrassSnowTiles.png"; Salt = 103 },
    @{ Owner = "Grass"; Group = "Rock"; Sheet = "IsometricTransitionGrassRockTiles.png"; Salt = 104 },
    @{ Owner = "Grass"; Group = "Swamp"; Sheet = "IsometricTransitionGrassSwampTiles.png"; Salt = 105 },
    @{ Owner = "Water"; Group = "Sand"; Sheet = "IsometricTransitionWaterSandTiles.png"; Salt = 106 },
    @{ Owner = "Snow"; Group = "Rock"; Sheet = "IsometricTransitionSnowRockTiles.png"; Salt = 107 },
    @{ Owner = "Floor"; Group = "DungeonWall"; Sheet = "IsometricTransitionDungeonFloorWallTiles.png"; Salt = 108 }
)

$script:OverlaySpecs = @{
    Forest = (AtlasCell "IsometricNatureTiles.png" 0 0)
    ForestDark = (AtlasCell "IsometricNatureTiles.png" 1 0)
    AutumnWood = (AtlasCell "IsometricNatureTiles.png" 3 0)
    Mountain = (AtlasCell "IsometricNatureTiles.png" 0 1)
    Hill = (AtlasCell "IsometricNatureTiles.png" 6 0)
    Cave = (AtlasCell "IsometricNatureTiles.png" 4 1)
    Pond = (AtlasCell "IsometricNatureTiles.png" 0 2)
    Settlement = (AtlasCell "IsometricSettlementTiles.png" 1 0)
    Building = (AtlasCell "IsometricSettlementTiles.png" 0 0)
    Inn = (AtlasCell "IsometricSettlementTiles.png" 2 0)
    Tower = (AtlasCell "IsometricSettlementTiles.png" 3 0)
    Market = (AtlasCell "IsometricSettlementTiles.png" 4 0)
    Temple = (AtlasCell "IsometricSettlementTiles.png" 5 0)
    Keep = (AtlasCell "IsometricSettlementTiles.png" 6 0)
    Landmark = (AtlasCell "IsometricSettlementTiles.png" 5 0)
    Door = (AtlasCell "IsometricDungeonTiles.png" 2 0)
    Gate = (AtlasCell "IsometricDungeonTiles.png" 3 0)
    Stairs = (AtlasCell "IsometricTravelAndMapTiles.png" 1 1)
    Portal = (AtlasCell "IsometricTerrainTiles.png" 6 7)
    Bridge = (AtlasCell "IsometricBattlefieldTiles.png" 2 0)
    Rubble = (AtlasCell "IsometricBattlefieldTiles.png" 0 0)
    Cover = (AtlasCell "IsometricBattlefieldTiles.png" 1 0)
    WallProp = (AtlasCell "IsometricBattlefieldTiles.png" 3 0)
    Ledge = (AtlasCell "IsometricBattlefieldTiles.png" 4 0)
    Trap = (AtlasCell "IsometricBattlefieldTiles.png" 5 0)
    PitProp = (AtlasCell "IsometricBattlefieldTiles.png" 6 0)
    LavaVent = (AtlasCell "IsometricBattlefieldTiles.png" 7 0)
    Ladder = (AtlasCell "IsometricBattlefieldTiles.png" 5 1)
    Tree = (AtlasCell "IsometricBattlefieldTiles.png" 7 1)
    Boulder = (AtlasCell "IsometricBattlefieldTiles.png" 2 1)
    Camp = (AtlasCell "IsometricBattlefieldTiles.png" 1 4)
    Cart = (AtlasCell "IsometricBattlefieldTiles.png" 2 4)
    Fence = (AtlasCell "IsometricBattlefieldTiles.png" 5 4)
    Palisade = (AtlasCell "IsometricBattlefieldTiles.png" 6 4)
    Altar = (AtlasCell "IsometricBattlefieldTiles.png" 4 5)
    Grate = (AtlasCell "IsometricBattlefieldTiles.png" 5 5)
    Crystal = (AtlasCell "IsometricTerrainTiles.png" 5 7)
}

function New-Grid([int]$Width, [int]$Height, [string]$BaseKind) {
    $base = New-Object string[] ($Width * $Height)
    $overlay = New-Object string[] ($Width * $Height)
    for ($i = 0; $i -lt $base.Length; ++$i) {
        $base[$i] = $BaseKind
        $overlay[$i] = ""
    }
    return [pscustomobject]@{ Width = $Width; Height = $Height; Base = $base; Overlay = $overlay }
}

function Test-InBounds($Grid, [int]$X, [int]$Y) {
    return $X -ge 0 -and $X -lt $Grid.Width -and $Y -ge 0 -and $Y -lt $Grid.Height
}

function Grid-Index($Grid, [int]$X, [int]$Y) {
    return ($Y * $Grid.Width) + $X
}

function Set-Base($Grid, [int]$X, [int]$Y, [string]$Kind) {
    if (Test-InBounds $Grid $X $Y) {
        $Grid.Base[(Grid-Index $Grid $X $Y)] = $Kind
    }
}

function Set-Overlay($Grid, [int]$X, [int]$Y, [string]$Kind) {
    if (Test-InBounds $Grid $X $Y) {
        $Grid.Overlay[(Grid-Index $Grid $X $Y)] = $Kind
    }
}

function Fill-Rect($Grid, [int]$X, [int]$Y, [int]$W, [int]$H, [string]$Kind) {
    for ($yy = $Y; $yy -lt $Y + $H; ++$yy) {
        for ($xx = $X; $xx -lt $X + $W; ++$xx) {
            Set-Base $Grid $xx $yy $Kind
        }
    }
}

function Prop-Rect($Grid, [int]$X, [int]$Y, [int]$W, [int]$H, [string]$Kind) {
    for ($yy = $Y; $yy -lt $Y + $H; ++$yy) {
        for ($xx = $X; $xx -lt $X + $W; ++$xx) {
            Set-Overlay $Grid $xx $yy $Kind
        }
    }
}

function Outline-Rect($Grid, [int]$X, [int]$Y, [int]$W, [int]$H, [string]$Kind) {
    for ($xx = $X; $xx -lt $X + $W; ++$xx) {
        Set-Base $Grid $xx $Y $Kind
        Set-Base $Grid $xx ($Y + $H - 1) $Kind
    }
    for ($yy = $Y; $yy -lt $Y + $H; ++$yy) {
        Set-Base $Grid $X $yy $Kind
        Set-Base $Grid ($X + $W - 1) $yy $Kind
    }
}

function Fill-Disc($Grid, [int]$CenterX, [int]$CenterY, [int]$Radius, [string]$Kind) {
    for ($yy = $CenterY - $Radius; $yy -le $CenterY + $Radius; ++$yy) {
        for ($xx = $CenterX - $Radius; $xx -le $CenterX + $Radius; ++$xx) {
            $dx = $xx - $CenterX
            $dy = $yy - $CenterY
            if (($dx * $dx + $dy * $dy) -le ($Radius * $Radius)) {
                Set-Base $Grid $xx $yy $Kind
            }
        }
    }
}

function Paint-Line($Grid, [int]$X0, [int]$Y0, [int]$X1, [int]$Y1, [string]$Kind, [int]$Radius = 0) {
    $dx = [Math]::Abs($X1 - $X0)
    $sx = if ($X0 -lt $X1) { 1 } else { -1 }
    $dy = -[Math]::Abs($Y1 - $Y0)
    $sy = if ($Y0 -lt $Y1) { 1 } else { -1 }
    $err = $dx + $dy
    while ($true) {
        for ($oy = -$Radius; $oy -le $Radius; ++$oy) {
            for ($ox = -$Radius; $ox -le $Radius; ++$ox) {
                if (($ox * $ox + $oy * $oy) -le ($Radius * $Radius)) {
                    Set-Base $Grid ($X0 + $ox) ($Y0 + $oy) $Kind
                }
            }
        }
        if ($X0 -eq $X1 -and $Y0 -eq $Y1) { break }
        $e2 = 2 * $err
        if ($e2 -ge $dy) { $err += $dy; $X0 += $sx }
        if ($e2 -le $dx) { $err += $dx; $Y0 += $sy }
    }
}

function Prop-Line($Grid, [int]$X0, [int]$Y0, [int]$X1, [int]$Y1, [string]$Kind) {
    $dx = [Math]::Abs($X1 - $X0)
    $sx = if ($X0 -lt $X1) { 1 } else { -1 }
    $dy = -[Math]::Abs($Y1 - $Y0)
    $sy = if ($Y0 -lt $Y1) { 1 } else { -1 }
    $err = $dx + $dy
    while ($true) {
        Set-Overlay $Grid $X0 $Y0 $Kind
        if ($X0 -eq $X1 -and $Y0 -eq $Y1) { break }
        $e2 = 2 * $err
        if ($e2 -ge $dy) { $err += $dy; $X0 += $sx }
        if ($e2 -le $dx) { $err += $dx; $Y0 += $sy }
    }
}

function Get-Variant([int]$X, [int]$Y, [int]$Salt) {
    $value = ($X * 73856093) -bxor ($Y * 19349663) -bxor ($Salt * 83492791)
    return (($value -band 0x7fffffff) % 4)
}

function Test-BaseConnection([string]$Kind, [string]$NeighborKind) {
    if ($Kind -eq "Road" -or $Kind -eq "Bridge") {
        return $NeighborKind -eq "Road" -or $NeighborKind -eq "Bridge"
    }
    return $NeighborKind -eq $Kind
}

function Get-Mask($Grid, [int]$X, [int]$Y, [string]$Kind) {
    $mask = 0
    if ($Y -gt 0 -and (Test-BaseConnection $Kind $Grid.Base[(Grid-Index $Grid $X ($Y - 1))])) { $mask = $mask -bor 1 }
    if ($X -lt $Grid.Width - 1 -and (Test-BaseConnection $Kind $Grid.Base[(Grid-Index $Grid ($X + 1) $Y)])) { $mask = $mask -bor 2 }
    if ($Y -lt $Grid.Height - 1 -and (Test-BaseConnection $Kind $Grid.Base[(Grid-Index $Grid $X ($Y + 1))])) { $mask = $mask -bor 4 }
    if ($X -gt 0 -and (Test-BaseConnection $Kind $Grid.Base[(Grid-Index $Grid ($X - 1) $Y)])) { $mask = $mask -bor 8 }
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

function Get-RelationMask($Grid, [int]$X, [int]$Y, [string]$Group) {
    $mask = 0
    if ($Y -gt 0 -and (Test-RelationMember $Grid.Base[(Grid-Index $Grid $X ($Y - 1))] $Group)) { $mask = $mask -bor 1 }
    if ($X -lt $Grid.Width - 1 -and (Test-RelationMember $Grid.Base[(Grid-Index $Grid ($X + 1) $Y)] $Group)) { $mask = $mask -bor 2 }
    if ($Y -lt $Grid.Height - 1 -and (Test-RelationMember $Grid.Base[(Grid-Index $Grid $X ($Y + 1))] $Group)) { $mask = $mask -bor 4 }
    if ($X -gt 0 -and (Test-RelationMember $Grid.Base[(Grid-Index $Grid ($X - 1) $Y)] $Group)) { $mask = $mask -bor 8 }
    return $mask
}

function Get-BaseSpec($Grid, [int]$X, [int]$Y) {
    $kind = $Grid.Base[(Grid-Index $Grid $X $Y)]
    if ($script:AutoSheets.ContainsKey($kind)) {
        $mask = Get-Mask $Grid $X $Y $kind
        $column = $mask % 8
        $variant = Get-Variant $X $Y $script:AutoSalts[$kind]
        $row = ([int][Math]::Floor($mask / 8) * 4) + $variant
        return AtlasCell $script:AutoSheets[$kind] $column $row
    }
    return AtlasCell $script:AutoSheets["Grass"] 0 0
}

function Get-TransitionSpecs($Grid, [int]$X, [int]$Y) {
    $kind = $Grid.Base[(Grid-Index $Grid $X $Y)]
    $specs = @()
    foreach ($rule in $script:TransitionRules) {
        if ($kind -ne $rule.Owner) { continue }
        $mask = Get-RelationMask $Grid $X $Y $rule.Group
        if ($mask -eq 0) { continue }
        $column = $mask % 8
        $variant = Get-Variant $X $Y $rule.Salt
        $row = ([int][Math]::Floor($mask / 8) * 4) + $variant
        $specs += (AtlasCell $rule.Sheet $column $row)
    }
    return ,$specs
}

function Draw-AtlasCell($Graphics, $Spec, [Drawing.Rectangle]$DestRect) {
    $sheet = Get-Sheet $Spec.Sheet
    $sourceRect = [Drawing.Rectangle]::new($Spec.Column * 128, $Spec.Row * 128, 128, 128)
    $Graphics.DrawImage($sheet, $DestRect, $sourceRect, [Drawing.GraphicsUnit]::Pixel)
}

function Render-Map([string]$Path, [int]$PixelWidth, [int]$PixelHeight, $Grid) {
    New-Item -ItemType Directory -Force -Path ([IO.Path]::GetDirectoryName((Resolve-Path -LiteralPath ".").Path + "\" + $Path)) | Out-Null
    $bitmap = [Drawing.Bitmap]::new($PixelWidth, $PixelHeight, [Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.Clear([Drawing.Color]::FromArgb(32, 37, 30))

    for ($y = 0; $y -lt $Grid.Height; ++$y) {
        for ($x = 0; $x -lt $Grid.Width; ++$x) {
            $left = [int][Math]::Floor($x * $PixelWidth / [double]$Grid.Width)
            $top = [int][Math]::Floor($y * $PixelHeight / [double]$Grid.Height)
            $right = [int][Math]::Ceiling(($x + 1) * $PixelWidth / [double]$Grid.Width)
            $bottom = [int][Math]::Ceiling(($y + 1) * $PixelHeight / [double]$Grid.Height)
            $dest = [Drawing.Rectangle]::new($left, $top, [Math]::Max(1, $right - $left), [Math]::Max(1, $bottom - $top))
            Draw-AtlasCell $graphics (Get-BaseSpec $Grid $x $y) $dest
            foreach ($transitionSpec in (Get-TransitionSpecs $Grid $x $y)) {
                Draw-AtlasCell $graphics $transitionSpec $dest
            }
        }
    }

    for ($y = 0; $y -lt $Grid.Height; ++$y) {
        for ($x = 0; $x -lt $Grid.Width; ++$x) {
            $kind = $Grid.Overlay[(Grid-Index $Grid $x $y)]
            if (-not $kind -or -not $script:OverlaySpecs.ContainsKey($kind)) { continue }
            $left = [int][Math]::Floor($x * $PixelWidth / [double]$Grid.Width)
            $top = [int][Math]::Floor($y * $PixelHeight / [double]$Grid.Height)
            $right = [int][Math]::Ceiling(($x + 1) * $PixelWidth / [double]$Grid.Width)
            $bottom = [int][Math]::Ceiling(($y + 1) * $PixelHeight / [double]$Grid.Height)
            $dest = [Drawing.Rectangle]::new($left, $top, [Math]::Max(1, $right - $left), [Math]::Max(1, $bottom - $top))
            Draw-AtlasCell $graphics $script:OverlaySpecs[$kind] $dest
        }
    }

    $graphics.Dispose()
    $bitmap.Save($Path, [Drawing.Imaging.ImageFormat]::Bmp)
    $bitmap.Dispose()
}

function Save-Map([string]$Root, [string]$File, [int]$PixelsW, [int]$PixelsH, [int]$GridW, [int]$GridH, [string]$Base, [scriptblock]$Build) {
    $grid = New-Grid $GridW $GridH $Base
    & $Build $grid
    $path = Join-Path $Root ("Assets/Maps/" + $File)
    Render-Map $path $PixelsW $PixelsH $grid
    Write-Host "Generated $path"
}

function Apply-SourceMap([string]$Root, [string]$SourceFile, [string]$OutputFile, [int]$PixelsW, [int]$PixelsH) {
    $sourcePath = Join-Path $Root ("Assets/Maps/Source/FantasyNeutral/" + $SourceFile)
    if (-not (Test-Path -LiteralPath $sourcePath)) {
        return
    }

    $outputPath = Join-Path $Root ("Assets/Maps/" + $OutputFile)
    $source = [Drawing.Image]::FromFile((Resolve-Path -LiteralPath $sourcePath).Path)
    $bitmap = [Drawing.Bitmap]::new($PixelsW, $PixelsH, [Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.Clear([Drawing.Color]::FromArgb(20, 22, 24))

    $scale = [Math]::Max($PixelsW / [double]$source.Width, $PixelsH / [double]$source.Height)
    $drawW = [int][Math]::Ceiling($source.Width * $scale)
    $drawH = [int][Math]::Ceiling($source.Height * $scale)
    $dest = [Drawing.Rectangle]::new(
        [int][Math]::Floor(($PixelsW - $drawW) / 2.0),
        [int][Math]::Floor(($PixelsH - $drawH) / 2.0),
        $drawW,
        $drawH
    )
    $graphics.DrawImage($source, $dest)

    $graphics.Dispose()
    $source.Dispose()
    $bitmap.Save($outputPath, [Drawing.Imaging.ImageFormat]::Bmp)
    $bitmap.Dispose()
    Write-Host "Applied AI-painted source $sourcePath -> $outputPath"
}

Save-Map $BlackwaterRoot "silted_crown_common_room.bmp" 900 620 18 12 "Wall" {
    param($g)
    Fill-Rect $g 1 1 16 10 "Floor"
    Outline-Rect $g 0 0 18 12 "Wall"
    Fill-Rect $g 2 8 5 2 "Dirt"
    Paint-Line $g 7 6 14 6 "Road" 0
    Prop-Rect $g 3 3 2 1 "Cover"
    Prop-Rect $g 8 3 4 1 "Cover"
    Prop-Rect $g 12 8 2 1 "Cover"
    Set-Overlay $g 1 5 "Door"
    Set-Overlay $g 15 2 "Ledge"
    Set-Overlay $g 4 8 "Camp"
    Set-Overlay $g 14 6 "Trap"
}

Save-Map $BlackwaterRoot "blackwater_town_ward.bmp" 1100 760 48 36 "Grass" {
    param($g)
    Fill-Disc $g 42 30 13 "Water"
    Fill-Rect $g 0 29 48 7 "Water"
    Fill-Disc $g 6 7 5 "Swamp"
    Fill-Rect $g 0 0 10 6 "Swamp"
    Paint-Line $g 4 27 42 9 "Road" 0
    Paint-Line $g 6 17 33 17 "Road" 0
    Paint-Line $g 18 5 18 31 "Road" 0
    Fill-Rect $g 16 14 8 6 "Dirt"
    Fill-Rect $g 31 21 8 3 "Stone"
    Prop-Rect $g 15 12 2 2 "Building"
    Prop-Rect $g 20 13 2 2 "Market"
    Prop-Rect $g 24 16 2 2 "Temple"
    Set-Overlay $g 12 9 "Keep"
    Set-Overlay $g 34 23 "Bridge"
    Set-Overlay $g 37 24 "Bridge"
    foreach ($p in @(@(4,10),@(8,11),@(11,28),@(27,8),@(29,9),@(39,7),@(43,12))) { Set-Overlay $g $p[0] $p[1] "Forest" }
    foreach ($p in @(@(2,4),@(3,5),@(5,3),@(7,4),@(9,5))) { Set-Overlay $g $p[0] $p[1] "ForestDark" }
    Set-Overlay $g 44 4 "Hill"
    Set-Overlay $g 45 5 "Mountain"
}

Save-Map $BlackwaterRoot "old_boathouse_ambush.bmp" 960 704 30 22 "Dirt" {
    param($g)
    Fill-Rect $g 0 0 30 22 "Swamp"
    Fill-Rect $g 0 9 30 5 "Water"
    Fill-Rect $g 7 5 8 7 "Dirt"
    Outline-Rect $g 8 6 6 5 "Wall"
    Fill-Rect $g 9 7 4 3 "Floor"
    Paint-Line $g 12 11 25 11 "Road" 0
    Prop-Line $g 14 10 23 10 "Bridge"
    Prop-Line $g 14 12 23 12 "Bridge"
    Set-Overlay $g 11 11 "Door"
    Prop-Rect $g 17 8 3 2 "Cover"
    Prop-Rect $g 21 13 3 2 "Cover"
    Set-Overlay $g 25 10 "Trap"
    foreach ($p in @(@(4,5),@(5,16),@(8,17),@(26,6),@(27,15))) { Set-Overlay $g $p[0] $p[1] "Forest" }
}

Save-Map $BlackwaterRoot "reedfen_marsh_causeway.bmp" 980 720 44 32 "Swamp" {
    param($g)
    Fill-Disc $g 8 7 6 "Water"
    Fill-Disc $g 33 23 7 "Water"
    Fill-Disc $g 20 18 5 "Water"
    Paint-Line $g 1 25 42 5 "Road" 1
    Paint-Line $g 20 18 34 23 "Stone" 0
    Set-Overlay $g 30 8 "Landmark"
    Set-Overlay $g 31 8 "Rubble"
    Set-Overlay $g 32 9 "Rubble"
    Set-Overlay $g 18 20 "Trap"
    foreach ($p in @(@(5,20),@(8,22),@(11,24),@(16,6),@(18,7),@(37,11),@(39,13),@(25,27))) { Set-Overlay $g $p[0] $p[1] "ForestDark" }
    foreach ($p in @(@(13,15),@(27,18),@(34,24))) { Set-Overlay $g $p[0] $p[1] "Pond" }
}

Save-Map $BlackwaterRoot "abandoned_bellfoundry_cellars.bmp" 980 720 38 28 "Wall" {
    param($g)
    Fill-Rect $g 2 2 14 10 "Floor"
    Fill-Rect $g 18 3 16 9 "Floor"
    Fill-Rect $g 6 15 26 10 "Floor"
    Paint-Line $g 15 7 19 7 "Floor" 0
    Paint-Line $g 19 11 19 16 "Floor" 0
    Outline-Rect $g 2 2 14 10 "Wall"
    Outline-Rect $g 18 3 16 9 "Wall"
    Outline-Rect $g 6 15 26 10 "Wall"
    Fill-Rect $g 24 6 5 3 "Lava"
    Fill-Rect $g 9 18 4 3 "Pit"
    Prop-Rect $g 21 17 3 2 "Cover"
    Set-Overlay $g 16 7 "Door"
    Set-Overlay $g 19 12 "Stairs"
    Set-Overlay $g 25 7 "LavaVent"
    Set-Overlay $g 13 5 "Rubble"
    Set-Overlay $g 28 20 "Altar"
}

Save-Map $BlackwaterRoot "lake_chapel_ruins.bmp" 1000 760 42 34 "Water" {
    param($g)
    Fill-Disc $g 21 17 13 "Grass"
    Fill-Disc $g 21 17 9 "Stone"
    Fill-Rect $g 15 10 12 12 "Floor"
    Outline-Rect $g 14 9 14 14 "Wall"
    Fill-Rect $g 17 13 8 6 "Floor"
    Paint-Line $g 21 0 21 10 "Bridge" 0
    Paint-Line $g 21 23 21 33 "Bridge" 0
    Set-Overlay $g 21 16 "Altar"
    Set-Overlay $g 25 22 "Stairs"
    Prop-Rect $g 14 9 2 2 "Rubble"
    Prop-Rect $g 26 20 2 2 "Rubble"
    foreach ($p in @(@(9,15),@(10,19),@(31,14),@(30,20),@(13,25))) { Set-Overlay $g $p[0] $p[1] "Forest" }
}

Save-Map $BlackwaterRoot "mnemosyne_bell_chamber.bmp" 980 720 36 28 "Chasm" {
    param($g)
    Fill-Disc $g 18 14 11 "Floor"
    Fill-Disc $g 18 14 6 "Stone"
    Fill-Disc $g 18 14 2 "Pit"
    Outline-Rect $g 5 3 26 22 "Wall"
    Paint-Line $g 18 3 18 11 "Floor" 1
    Paint-Line $g 6 14 15 14 "Floor" 1
    Paint-Line $g 21 14 30 14 "Floor" 1
    Set-Overlay $g 18 14 "Portal"
    Set-Overlay $g 18 9 "Altar"
    Set-Overlay $g 11 14 "Crystal"
    Set-Overlay $g 25 14 "Crystal"
    Prop-Rect $g 8 6 2 2 "Rubble"
    Prop-Rect $g 27 20 2 2 "Rubble"
}

Save-Map $ClockworkRoot "tallowgate_rail_terminus.bmp" 960 640 36 24 "Stone" {
    param($g)
    Fill-Rect $g 0 0 36 24 "Dirt"
    Fill-Rect $g 0 8 36 8 "Stone"
    Paint-Line $g 1 10 34 10 "Road" 0
    Paint-Line $g 1 14 34 14 "Road" 0
    Fill-Rect $g 4 4 7 5 "Floor"
    Fill-Rect $g 24 4 6 5 "Floor"
    Outline-Rect $g 4 4 7 5 "Wall"
    Outline-Rect $g 24 4 6 5 "Wall"
    Set-Overlay $g 8 8 "Door"
    Set-Overlay $g 27 8 "Tower"
    Prop-Rect $g 13 6 4 2 "Cover"
    Prop-Rect $g 19 16 5 2 "Cover"
    Set-Overlay $g 31 12 "Bridge"
}

Save-Map $ClockworkRoot "holtz_vane_foundry_floor.bmp" 960 640 34 24 "Wall" {
    param($g)
    Fill-Rect $g 2 2 30 20 "Floor"
    Outline-Rect $g 2 2 30 20 "Wall"
    Fill-Rect $g 4 4 5 15 "Lava"
    Fill-Rect $g 22 4 7 5 "Lava"
    Paint-Line $g 10 6 28 18 "Road" 0
    Prop-Line $g 10 7 28 19 "Bridge"
    Prop-Rect $g 12 13 3 2 "Cover"
    Prop-Rect $g 18 8 3 2 "Cover"
    Set-Overlay $g 29 18 "Trap"
    Set-Overlay $g 17 12 "LavaVent"
}

Save-Map $ClockworkRoot "royal_patent_office.bmp" 900 640 30 24 "Wall" {
    param($g)
    Fill-Rect $g 2 2 26 20 "Floor"
    Outline-Rect $g 2 2 26 20 "Wall"
    Fill-Rect $g 5 4 8 6 "Stone"
    Fill-Rect $g 16 4 9 6 "Stone"
    Fill-Rect $g 6 13 18 6 "Floor"
    Prop-Line $g 5 5 12 5 "Cover"
    Prop-Line $g 5 8 12 8 "Cover"
    Prop-Line $g 17 5 24 5 "Cover"
    Prop-Line $g 17 8 24 8 "Cover"
    Set-Overlay $g 15 11 "Gate"
    Set-Overlay $g 22 16 "Trap"
}

Save-Map $ClockworkRoot "sky_tram_switch_tower.bmp" 900 640 32 20 "Chasm" {
    param($g)
    Fill-Rect $g 3 8 26 4 "Stone"
    Fill-Rect $g 5 5 8 10 "Floor"
    Fill-Rect $g 22 4 5 12 "Floor"
    Outline-Rect $g 5 5 8 10 "Wall"
    Outline-Rect $g 22 4 5 12 "Wall"
    Paint-Line $g 13 10 22 10 "Road" 0
    Prop-Line $g 13 10 22 10 "Bridge"
    Set-Overlay $g 9 5 "Ladder"
    Set-Overlay $g 24 15 "Stairs"
    Set-Overlay $g 17 9 "Trap"
}

Save-Map $ClockworkRoot "porcelain_countess_salon.bmp" 900 640 32 24 "Wall" {
    param($g)
    Fill-Rect $g 2 2 28 20 "Floor"
    Outline-Rect $g 2 2 28 20 "Wall"
    Fill-Rect $g 4 4 10 7 "Stone"
    Fill-Rect $g 17 4 10 7 "Grass"
    Fill-Rect $g 5 14 20 5 "Floor"
    Prop-Rect $g 7 6 3 2 "Cover"
    Prop-Rect $g 18 6 2 2 "Tree"
    Prop-Rect $g 22 6 2 2 "Tree"
    Set-Overlay $g 25 18 "Door"
    Set-Overlay $g 13 16 "Trap"
}

Save-Map $ClockworkRoot "boiler_choir_underworks.bmp" 960 700 36 26 "Wall" {
    param($g)
    Fill-Rect $g 2 2 32 22 "Floor"
    Outline-Rect $g 2 2 32 22 "Wall"
    Fill-Rect $g 5 6 26 4 "Water"
    Fill-Rect $g 7 15 18 4 "Lava"
    Paint-Line $g 6 5 30 20 "Road" 0
    Prop-Line $g 6 5 30 20 "Bridge"
    Prop-Rect $g 10 11 3 2 "Cover"
    Prop-Rect $g 24 11 3 2 "Cover"
    Set-Overlay $g 18 13 "Trap"
    Set-Overlay $g 30 20 "Altar"
}

Save-Map $ClockworkRoot "glasshouse_canal_market.bmp" 900 640 34 22 "Stone" {
    param($g)
    Fill-Rect $g 0 8 34 6 "Water"
    Fill-Rect $g 0 0 34 8 "Grass"
    Fill-Rect $g 0 14 34 8 "Stone"
    Paint-Line $g 2 6 31 18 "Road" 0
    Set-Overlay $g 9 10 "Bridge"
    Set-Overlay $g 18 11 "Bridge"
    Set-Overlay $g 27 12 "Bridge"
    Prop-Rect $g 5 4 4 2 "Market"
    Prop-Rect $g 14 16 5 2 "Cover"
    Prop-Rect $g 22 16 4 2 "Market"
    foreach ($p in @(@(3,3),@(11,3),@(28,4))) { Set-Overlay $g $p[0] $p[1] "Tree" }
}

Save-Map $ClockworkRoot "meridian_station_engine_core.bmp" 1024 720 40 28 "Wall" {
    param($g)
    Fill-Rect $g 2 2 36 24 "Floor"
    Outline-Rect $g 2 2 36 24 "Wall"
    Fill-Rect $g 4 10 32 5 "Stone"
    Paint-Line $g 3 12 36 12 "Road" 0
    Paint-Line $g 20 3 20 24 "Road" 0
    Fill-Disc $g 27 18 4 "Lava"
    Set-Overlay $g 27 18 "Portal"
    Set-Overlay $g 20 12 "Altar"
    Prop-Rect $g 6 5 4 2 "Cover"
    Prop-Rect $g 11 20 4 2 "Cover"
    Set-Overlay $g 34 20 "Trap"
}

Apply-SourceMap $BlackwaterRoot "silted_crown_common_room.png" "silted_crown_common_room.bmp" 900 620
Apply-SourceMap $BlackwaterRoot "blackwater_town_ward.png" "blackwater_town_ward.bmp" 1100 760
Apply-SourceMap $BlackwaterRoot "old_boathouse_ambush.png" "old_boathouse_ambush.bmp" 960 704
Apply-SourceMap $BlackwaterRoot "reedfen_marsh_causeway.png" "reedfen_marsh_causeway.bmp" 980 720
Apply-SourceMap $BlackwaterRoot "abandoned_bellfoundry_cellars.png" "abandoned_bellfoundry_cellars.bmp" 980 720
Apply-SourceMap $BlackwaterRoot "lake_chapel_ruins.png" "lake_chapel_ruins.bmp" 1000 760
Apply-SourceMap $BlackwaterRoot "mnemosyne_bell_chamber.png" "mnemosyne_bell_chamber.bmp" 980 720

foreach ($sheet in $script:SheetCache.Values) {
    $sheet.Dispose()
}

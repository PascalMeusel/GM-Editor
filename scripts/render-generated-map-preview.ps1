param(
    [string]$ExporterPath = "build/map_generation_preview_export.exe",
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$OutputDirectory = "build/chat-map-preview",
    [string]$Preset = "Forest",
    [int]$Seed = 240529,
    [int]$TileSize = 48,
    [int]$Width = 24,
    [int]$Height = 18,
    [switch]$Open
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

$sourceRootPath = Resolve-TilesetPath $SourceRoot
$outputDirectoryPath = Resolve-TilesetPath $OutputDirectory
$exporterResolvedPath = Resolve-TilesetPath $ExporterPath
New-Item -ItemType Directory -Force -Path $outputDirectoryPath | Out-Null

if (-not (Test-Path -LiteralPath $exporterResolvedPath)) {
    throw "Missing exporter executable: $exporterResolvedPath"
}

$safePreset = ($Preset -replace '[^A-Za-z0-9_-]', '_').ToLowerInvariant()
$jsonPath = Join-Path $outputDirectoryPath "generated_map_${safePreset}_${Seed}.json"
$pngPath = Join-Path $outputDirectoryPath "generated_map_${safePreset}_${Seed}.png"

& $exporterResolvedPath --out $jsonPath --preset $Preset --seed $Seed --width $Width --height $Height | Write-Host
if ($LASTEXITCODE -ne 0) {
    throw "Map generation export failed with exit code $LASTEXITCODE"
}

$map = Get-Content -Raw -LiteralPath $jsonPath | ConvertFrom-Json
$bundle = Get-TilesetV2ManifestBundle $sourceRootPath
$tileAssets = @(Get-TilesetManifestAssets $bundle.Tiles)
$wayAssets = @(Get-TilesetManifestAssets $bundle.Ways)
$structureAssets = @(Get-TilesetManifestAssets $bundle.Taxonomy)

function Get-Cell($Map, [int]$X, [int]$Y) {
    if ($Y -lt 0 -or $Y -ge [int]$Map.height -or $X -lt 0 -or $X -ge [int]$Map.width) {
        return $null
    }
    return $Map.cells[$Y][$X]
}

function Test-RoadLike([int]$Way) {
    return $Way -in @(1, 2, 3)
}

function Test-WaterLike([int]$Way) {
    return $Way -in @(4, 5)
}

function Test-InteriorLike([int]$Way) {
    return $Way -in @(6, 7)
}

function Test-WayConnects([int]$OwnerWay, $Neighbor) {
    if ($null -eq $Neighbor -or $OwnerWay -eq 0 -or [int]$Neighbor.way -eq 0) {
        return $false
    }
    $neighborWay = [int]$Neighbor.way
    if ($OwnerWay -eq $neighborWay) { return $true }
    if ((Test-RoadLike $OwnerWay) -and (Test-RoadLike $neighborWay)) { return $true }
    if ((Test-WaterLike $OwnerWay) -and (Test-WaterLike $neighborWay)) { return $true }
    if ((Test-InteriorLike $OwnerWay) -and (Test-InteriorLike $neighborWay)) { return $true }
    return (Test-WaterLike $OwnerWay) -and ([int]$Neighbor.kind -eq 26) -and (Test-RoadLike $neighborWay)
}

function Get-WayMask8($Map, [int]$X, [int]$Y) {
    $cell = Get-Cell $Map $X $Y
    if ($null -eq $cell) { return 0 }
    $way = [int]$cell.way
    $mask = 0
    if (Test-WayConnects $way (Get-Cell $Map $X ($Y - 1))) { $mask = $mask -bor 1 }
    if (Test-WayConnects $way (Get-Cell $Map ($X + 1) ($Y - 1))) { $mask = $mask -bor 2 }
    if (Test-WayConnects $way (Get-Cell $Map ($X + 1) $Y)) { $mask = $mask -bor 4 }
    if (Test-WayConnects $way (Get-Cell $Map ($X + 1) ($Y + 1))) { $mask = $mask -bor 8 }
    if (Test-WayConnects $way (Get-Cell $Map $X ($Y + 1))) { $mask = $mask -bor 16 }
    if (Test-WayConnects $way (Get-Cell $Map ($X - 1) ($Y + 1))) { $mask = $mask -bor 32 }
    if (Test-WayConnects $way (Get-Cell $Map ($X - 1) $Y)) { $mask = $mask -bor 64 }
    if (Test-WayConnects $way (Get-Cell $Map ($X - 1) ($Y - 1))) { $mask = $mask -bor 128 }
    return $mask
}

function Test-KindConnects([int]$OwnerKind, $Neighbor) {
    if ($null -eq $Neighbor) { return $false }
    $neighborKind = [int]$Neighbor.kind
    if ($OwnerKind -eq 5 -or $OwnerKind -eq 26) {
        return $neighborKind -eq 5 -or $neighborKind -eq 26
    }
    return $OwnerKind -eq $neighborKind
}

function Get-KindMask4($Map, [int]$X, [int]$Y, [int]$Kind) {
    $mask = 0
    if (Test-KindConnects $Kind (Get-Cell $Map $X ($Y - 1))) { $mask = $mask -bor 1 }
    if (Test-KindConnects $Kind (Get-Cell $Map ($X + 1) $Y)) { $mask = $mask -bor 2 }
    if (Test-KindConnects $Kind (Get-Cell $Map $X ($Y + 1))) { $mask = $mask -bor 4 }
    if (Test-KindConnects $Kind (Get-Cell $Map ($X - 1) $Y)) { $mask = $mask -bor 8 }
    return $mask
}

function Get-TerrainFamily([int]$Kind, [int]$Way = 0) {
    switch ($Kind) {
        0 { return "grass_open" }
        5 {
            if ($Way -eq 3) { return "stone_paved" }
            return "dirt"
        }
        15 { return "dirt" }
        16 { return "stone_paved" }
        1 { return "water_shallow" }
        8 { return "dungeon_floor_stone" }
        11 { return "cave_floor" }
        17 { return "snow" }
        18 { return "desert_dune" }
        19 { return "swamp" }
        21 { return "ice" }
        25 { return "void_chasm" }
    }
    return "grass_open"
}

function Get-WayFamily([int]$Way) {
    switch ($Way) {
        1 { return "way_trail_narrow" }
        2 { return "way_dirt_road" }
        3 { return "way_stone_road" }
        4 { return "way_river_stream" }
        5 { return "way_canal" }
        6 { return "way_dungeon_corridor" }
        7 { return "way_cave_tunnel" }
    }
    return ""
}

function Select-AssetStable($Assets, [scriptblock]$Predicate, [int]$X, [int]$Y, [int]$Salt) {
    $pool = @($Assets | Where-Object {
        (& $Predicate $_) -and
        (Get-TilesetProperty $_ "status" "") -in @("candidate", "accepted")
    } | Sort-Object { Get-TilesetProperty $_ "id" "" })
    if ($pool.Count -eq 0) { return $null }
    return $pool[(Get-TilesetStableIndex $X $Y $Salt $pool.Count)]
}

function Get-WayFallbackMasks([int]$Mask) {
    $masks = @($Mask)
    $cardinalMask = $Mask -band 0x55
    if ($cardinalMask -gt 0 -and $cardinalMask -notin $masks) {
        $masks += $cardinalMask
    }
    $count = 0
    $cursor = $cardinalMask
    while ($cursor -ne 0) {
        $count += ($cursor -band 1)
        $cursor = $cursor -shr 1
    }
    if ($count -ge 3 -and 0x55 -notin $masks) {
        $masks += 0x55
    }
    return $masks
}

function Draw-Asset($Graphics, $Asset, [Drawing.Rectangle]$Destination, [int]$RotationQuarterTurns = 0) {
    if ($null -eq $Asset) { return $false }
    $sourcePath = Get-TilesetAssetSourcePath $Asset
    if (-not (Test-Path -LiteralPath $sourcePath)) { return $false }
    $bitmap = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $sourcePath).Path)
    try {
        $turns = ((($RotationQuarterTurns % 4) + 4) % 4)
        if ($turns -eq 1) { $bitmap.RotateFlip([Drawing.RotateFlipType]::Rotate90FlipNone) }
        elseif ($turns -eq 2) { $bitmap.RotateFlip([Drawing.RotateFlipType]::Rotate180FlipNone) }
        elseif ($turns -eq 3) { $bitmap.RotateFlip([Drawing.RotateFlipType]::Rotate270FlipNone) }
        $Graphics.DrawImage($bitmap, $Destination, 0, 0, $bitmap.Width, $bitmap.Height, [Drawing.GraphicsUnit]::Pixel)
    }
    finally {
        $bitmap.Dispose()
    }
    return $true
}

function Draw-FallbackSymbol($Graphics, [Drawing.Rectangle]$Rect, [int]$Kind) {
    $cx = $Rect.X + ($Rect.Width / 2)
    $cy = $Rect.Y + ($Rect.Height / 2)
    $brush = $null
    $pen = $null
    try {
        switch ($Kind) {
            2 {
                $brush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(210, 34, 96, 48))
                $Graphics.FillEllipse($brush, $cx - 10, $cy - 13, 20, 22)
                $brush.Color = [Drawing.Color]::FromArgb(230, 80, 54, 30)
                $Graphics.FillRectangle($brush, $cx - 2, $cy + 3, 4, 10)
            }
            3 {
                $brush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(225, 122, 124, 120))
                $points = [Drawing.Point[]]@(
                    [Drawing.Point]::new([int]($cx - 16), [int]($cy + 15)),
                    [Drawing.Point]::new([int]$cx, [int]($cy - 17)),
                    [Drawing.Point]::new([int]($cx + 17), [int]($cy + 15))
                )
                $Graphics.FillPolygon($brush, $points)
            }
            4 {
                $brush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(220, 106, 135, 72))
                $Graphics.FillEllipse($brush, $cx - 15, $cy - 10, 30, 20)
            }
            default {
                if ($Kind -notin @(0, 1, 5, 15, 16, 17, 18, 19, 21, 25, 26)) {
                    $pen = [Drawing.Pen]::new([Drawing.Color]::FromArgb(210, 238, 230, 180), 2)
                    $Graphics.DrawEllipse($pen, $cx - 10, $cy - 10, 20, 20)
                }
            }
        }
    }
    finally {
        if ($null -ne $brush) { $brush.Dispose() }
        if ($null -ne $pen) { $pen.Dispose() }
    }
}

function Get-FallbackTerrainColor([int]$Kind, [int]$Way = 0) {
    switch ($Kind) {
        0 { return [Drawing.Color]::FromArgb(255, 96, 134, 64) }
        1 { return [Drawing.Color]::FromArgb(255, 67, 148, 184) }
        2 { return [Drawing.Color]::FromArgb(255, 74, 126, 58) }
        3 { return [Drawing.Color]::FromArgb(255, 116, 118, 111) }
        4 { return [Drawing.Color]::FromArgb(255, 118, 143, 75) }
        5 {
            if ($Way -eq 3) { return [Drawing.Color]::FromArgb(255, 122, 121, 112) }
            return [Drawing.Color]::FromArgb(255, 138, 98, 55)
        }
        6 { return [Drawing.Color]::FromArgb(255, 132, 95, 66) }
        7 { return [Drawing.Color]::FromArgb(255, 132, 95, 66) }
        8 { return [Drawing.Color]::FromArgb(255, 102, 96, 88) }
        9 { return [Drawing.Color]::FromArgb(255, 52, 56, 62) }
        10 { return [Drawing.Color]::FromArgb(255, 153, 102, 54) }
        11 { return [Drawing.Color]::FromArgb(255, 92, 80, 64) }
        12 { return [Drawing.Color]::FromArgb(255, 132, 118, 154) }
        13 { return [Drawing.Color]::FromArgb(255, 167, 65, 59) }
        14 { return [Drawing.Color]::FromArgb(255, 160, 124, 70) }
        15 { return [Drawing.Color]::FromArgb(255, 143, 105, 66) }
        16 { return [Drawing.Color]::FromArgb(255, 124, 124, 116) }
        17 { return [Drawing.Color]::FromArgb(255, 211, 228, 231) }
        18 { return [Drawing.Color]::FromArgb(255, 201, 168, 103) }
        19 { return [Drawing.Color]::FromArgb(255, 83, 111, 73) }
        20 { return [Drawing.Color]::FromArgb(255, 205, 76, 40) }
        21 { return [Drawing.Color]::FromArgb(255, 158, 211, 224) }
        22 { return [Drawing.Color]::FromArgb(255, 34, 30, 28) }
        23 { return [Drawing.Color]::FromArgb(255, 113, 106, 96) }
        24 { return [Drawing.Color]::FromArgb(255, 99, 121, 92) }
        25 { return [Drawing.Color]::FromArgb(255, 30, 33, 40) }
        26 { return [Drawing.Color]::FromArgb(255, 112, 82, 48) }
    }
    return [Drawing.Color]::FromArgb(255, 96, 134, 64)
}

function Test-BridgeCell($Cell) {
    if ($null -eq $Cell) { return $false }
    return (Test-RoadLike ([int]$Cell.way)) -and ([int]$Cell.kind -in @(1, 19, 26))
}

function Get-BridgeSpanPosition($Map, [int]$X, [int]$Y) {
    $north = Get-Cell $Map $X ($Y - 1)
    $east = Get-Cell $Map ($X + 1) $Y
    $south = Get-Cell $Map $X ($Y + 1)
    $west = Get-Cell $Map ($X - 1) $Y
    $northBridge = Test-BridgeCell $north
    $eastBridge = Test-BridgeCell $east
    $southBridge = Test-BridgeCell $south
    $westBridge = Test-BridgeCell $west
    $northApproach = ($null -ne $north) -and (Test-RoadLike ([int]$north.way)) -and -not $northBridge
    $eastApproach = ($null -ne $east) -and (Test-RoadLike ([int]$east.way)) -and -not $eastBridge
    $southApproach = ($null -ne $south) -and (Test-RoadLike ([int]$south.way)) -and -not $southBridge
    $westApproach = ($null -ne $west) -and (Test-RoadLike ([int]$west.way)) -and -not $westBridge
    $bridgeCount = @($northBridge, $eastBridge, $southBridge, $westBridge | Where-Object { $_ }).Count
    if (($northApproach -and $southBridge) -or ($westApproach -and $eastBridge)) { return "threshold_a" }
    if (($southApproach -and $northBridge) -or ($eastApproach -and $westBridge)) { return "threshold_b" }
    if ($bridgeCount -ge 2) {
        if ((($X + $Y) % 2) -eq 0) { return "mid_01" }
        return "mid_02"
    }
    if (($northApproach -and $southApproach) -or ($eastApproach -and $westApproach)) {
        if ((($X + $Y) % 2) -eq 0) { return "mid_01" }
        return "mid_02"
    }
    if ($northApproach -or $westApproach) { return "threshold_a" }
    if ($southApproach -or $eastApproach) { return "threshold_b" }
    if ($bridgeCount -eq 1) {
        if ($northBridge -or $westBridge) { return "threshold_b" }
        return "threshold_a"
    }
    return "mid_01"
}

function Get-BridgeRotation($Map, [int]$X, [int]$Y) {
    $east = Get-Cell $Map ($X + 1) $Y
    $west = Get-Cell $Map ($X - 1) $Y
    if ((Test-WayConnects ([int](Get-Cell $Map $X $Y).way) $east) -or (Test-WayConnects ([int](Get-Cell $Map $X $Y).way) $west)) {
        return 1
    }
    return 0
}

function Select-BaseAsset($Map, [int]$X, [int]$Y) {
    $cell = Get-Cell $Map $X $Y
    $kind = [int]$cell.kind
    $baseKind = if (Test-BridgeCell $cell) { 1 } else { $kind }
    if ($baseKind -in @(2, 3, 4, 6, 7, 9, 10, 12, 13, 14, 20, 22, 23, 24)) {
        $baseKind = 0
    }
    $family = Get-TerrainFamily $baseKind ([int]$cell.way)
    $mask = Get-KindMask4 $Map $X $Y $baseKind
    $asset = Select-TilesetAsset $tileAssets $family @("same_auto") $mask $X $Y 67
    if ($null -eq $asset) {
        $asset = Select-TilesetAsset $tileAssets $family @("base") $null $X $Y 71
    }
    return $asset
}

function Select-WayAsset($Map, [int]$X, [int]$Y) {
    $cell = Get-Cell $Map $X $Y
    $way = [int]$cell.way
    if ($way -eq 0) { return [pscustomobject]@{ Asset = $null; Rotation = 0 } }
    if (Test-BridgeCell $cell) {
        $owner = if ($way -eq 3 -or $way -eq 5) { "bridge_stone" } else { "bridge_wood_simple" }
        $span = Get-BridgeSpanPosition $Map $X $Y
        $asset = Select-AssetStable $wayAssets {
            param($a)
            (Get-TilesetProperty $a "asset_class" "") -eq "bridge_span" -and
            (Get-TilesetProperty $a "owner" "") -eq $owner -and
            (Get-TilesetProperty $a "span_position" "") -eq $span
        } $X $Y 233
        return [pscustomobject]@{ Asset = $asset; Rotation = (Get-BridgeRotation $Map $X $Y) }
    }
    $owner = Get-WayFamily $way
    if ([string]::IsNullOrWhiteSpace($owner)) { return [pscustomobject]@{ Asset = $null; Rotation = 0 } }
    $ground = Get-TerrainFamily ([int]$cell.kind) $way
    $mask = Get-WayMask8 $Map $X $Y
    $asset = $null
    foreach ($candidateMask in (Get-WayFallbackMasks $mask)) {
        $asset = Select-AssetStable $wayAssets {
            param($a)
            (Get-TilesetProperty $a "asset_class" "") -eq "way_segment" -and
            (Get-TilesetProperty $a "owner" "") -eq $owner -and
            [int](Get-TilesetProperty $a "mask" -1) -eq $candidateMask -and
            (Get-TilesetProperty (Get-TilesetProperty $a "context" $null) "ground" "") -eq $ground
        } $X $Y 181
        if ($null -ne $asset) { break }
    }
    return [pscustomobject]@{ Asset = $asset; Rotation = 0 }
}

function Select-StructureAsset($Map, [int]$X, [int]$Y) {
    $cell = Get-Cell $Map $X $Y
    if ([int]$cell.kind -notin @(6, 7, 14)) { return $null }
    $owner = switch ([int]$cell.kind) {
        6 { "village_cluster" }
        7 { "settlement_house" }
        14 { "guard_tower" }
        default { "" }
    }
    if ([string]::IsNullOrWhiteSpace($owner)) { return $null }
    $directions = @(
        @{ Name = "n"; X = 0; Y = -1 },
        @{ Name = "e"; X = 1; Y = 0 },
        @{ Name = "s"; X = 0; Y = 1 },
        @{ Name = "w"; X = -1; Y = 0 }
    )
    foreach ($direction in $directions) {
        $neighbor = Get-Cell $Map ($X + $direction.X) ($Y + $direction.Y)
        if ($null -eq $neighbor -or [int]$neighbor.way -eq 0) { continue }
        $wayKey = Get-WayFamily ([int]$neighbor.way)
        if ([string]::IsNullOrWhiteSpace($wayKey)) { continue }
        $ground = Get-TerrainFamily ([int]$neighbor.kind) ([int]$neighbor.way)
        $asset = Select-AssetStable $structureAssets {
            param($a)
            (Get-TilesetProperty $a "asset_class" "") -eq "structure" -and
            (Get-TilesetProperty $a "owner" "") -eq $owner -and
            (Get-TilesetProperty (Get-TilesetProperty $a "context" $null) "ground" "") -eq $ground -and
            (Get-TilesetProperty (Get-TilesetProperty $a "context" $null) "entrance_type" "") -eq $wayKey -and
            (Get-TilesetProperty (Get-TilesetProperty $a "context" $null) "orientation" "") -eq $direction.Name
        } $X $Y 307
        if ($null -ne $asset) { return $asset }
    }
    return Select-AssetStable $structureAssets {
        param($a)
        (Get-TilesetProperty $a "asset_class" "") -eq "structure" -and
        (Get-TilesetProperty $a "owner" "") -eq $owner -and
        (Get-TilesetProperty $a "role" "") -eq "ground_only"
    } $X $Y 311
}

$imageWidth = [int]$map.width * $TileSize
$imageHeight = [int]$map.height * $TileSize
$sheet = New-TilesetReviewBitmap $imageWidth $imageHeight "#22282d"
try {
    for ($y = 0; $y -lt [int]$map.height; ++$y) {
        for ($x = 0; $x -lt [int]$map.width; ++$x) {
            $dest = [Drawing.Rectangle]::new($x * $TileSize, $y * $TileSize, $TileSize, $TileSize)
            $cell = Get-Cell $map $x $y
            $fallbackBrush = [Drawing.SolidBrush]::new((Get-FallbackTerrainColor ([int]$cell.kind) ([int]$cell.way)))
            try {
                $sheet.Graphics.FillRectangle($fallbackBrush, $dest)
            }
            finally {
                $fallbackBrush.Dispose()
            }
            [void](Draw-Asset $sheet.Graphics (Select-BaseAsset $map $x $y) $dest)
        }
    }
    for ($y = 0; $y -lt [int]$map.height; ++$y) {
        for ($x = 0; $x -lt [int]$map.width; ++$x) {
            $dest = [Drawing.Rectangle]::new($x * $TileSize, $y * $TileSize, $TileSize, $TileSize)
            $cell = Get-Cell $map $x $y
            Draw-FallbackSymbol $sheet.Graphics $dest ([int]$cell.kind)
            $way = Select-WayAsset $map $x $y
            [void](Draw-Asset $sheet.Graphics $way.Asset $dest $way.Rotation)
            [void](Draw-Asset $sheet.Graphics (Select-StructureAsset $map $x $y) $dest)
        }
    }
    $gridPen = [Drawing.Pen]::new([Drawing.Color]::FromArgb(28, 255, 255, 255), 1)
    try {
        for ($x = 0; $x -le [int]$map.width; ++$x) {
            $sheet.Graphics.DrawLine($gridPen, $x * $TileSize, 0, $x * $TileSize, $imageHeight)
        }
        for ($y = 0; $y -le [int]$map.height; ++$y) {
            $sheet.Graphics.DrawLine($gridPen, 0, $y * $TileSize, $imageWidth, $y * $TileSize)
        }
    }
    finally {
        $gridPen.Dispose()
    }
    $sheet.Bitmap.Save($pngPath, [Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $sheet.Graphics.Dispose()
    $sheet.Bitmap.Dispose()
}

$summary = [pscustomobject]@{
    preset = $Preset
    seed = $Seed
    width = [int]$map.width
    height = [int]$map.height
    json = ConvertTo-TilesetRelativePath $jsonPath
    png = ConvertTo-TilesetRelativePath $pngPath
}
$summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outputDirectoryPath "generated_map_${safePreset}_${Seed}_summary.json") -Encoding UTF8
Write-Host "Rendered generated map preview: $pngPath"
if ($Open) {
    Invoke-Item $pngPath
}

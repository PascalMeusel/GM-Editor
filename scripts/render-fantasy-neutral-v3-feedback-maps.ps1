param(
    [Parameter(Mandatory=$true)][string]$InputDirectory,
    [string]$OutputDirectory = "",
    [int]$PixelsPerCell = 32
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
function Project-Path([string]$Path) {
    if ([IO.Path]::IsPathRooted($Path)) { return [IO.Path]::GetFullPath($Path) }
    return [IO.Path]::GetFullPath((Join-Path $repositoryRoot $Path))
}
function Stable-Index([int]$Seed, [int]$X, [int]$Y, [int]$Salt, [int]$Count) {
    if ($Count -le 1) { return 0 }
    [uint64]$value = ([uint64]([uint32]$Seed) * 73856093) + ([uint64]([uint32]($X + 8192)) * 19349663) + ([uint64]([uint32]($Y + 8192)) * 83492791) + ([uint64]([uint32]$Salt) * 2654435761)
    return [int]($value % [uint64]$Count)
}

$inputPath = Project-Path $InputDirectory
$outputPath = if ([string]::IsNullOrWhiteSpace($OutputDirectory)) { $inputPath } else { Project-Path $OutputDirectory }
New-Item -ItemType Directory -Force -Path $outputPath | Out-Null
$assetRoot = Project-Path "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3"
$manifest = Get-Content -Raw -LiteralPath (Join-Path $assetRoot "pack_manifest.json") | ConvertFrom-Json
$snapshot = Get-Content -Raw -LiteralPath (Join-Path $assetRoot "production_style_snapshot.json") | ConvertFrom-Json
if ([string]$snapshot.state -ne "production" -or [int]$snapshot.asset_count -ne 424) { throw "FantasyNeutral V3 production snapshot is not complete." }
$snapshotIds = @{}; foreach ($entry in @($snapshot.assets)) { $snapshotIds[[string]$entry.id] = $true }
$assetsByRole = @{}
foreach ($asset in @($manifest.assets)) {
    if (-not $snapshotIds.ContainsKey([string]$asset.id) -or [string]$asset.status -ne "accepted") { continue }
    $role = [string]$asset.semantic_role
    if (-not $assetsByRole.ContainsKey($role)) { $assetsByRole[$role] = [Collections.Generic.List[object]]::new() }
    $assetsByRole[$role].Add($asset)
}
$imageCache = @{}
function Asset-For([string]$Role, [int]$Seed, [int]$X, [int]$Y, [int]$Salt=0) {
    if (-not $assetsByRole.ContainsKey($Role)) { return $null }
    $items = $assetsByRole[$Role]
    return $items[(Stable-Index $Seed $X $Y $Salt $items.Count)]
}
function Image-For($Asset) {
    $id = [string]$Asset.id
    if (-not $imageCache.ContainsKey($id)) {
        $path = Join-Path $assetRoot ([string]$Asset.file).Replace('/', '\')
        $imageCache[$id] = [Drawing.Bitmap]::new($path)
    }
    return $imageCache[$id]
}
function Record-Usage($Usage, $Asset) {
    if ($null -eq $Asset) { return }
    $id = [string]$Asset.id
    if (-not $Usage.ContainsKey($id)) { $Usage[$id] = 0 }
    $Usage[$id] = [int]$Usage[$id] + 1
}
function Draw-Material($Graphics, $Usage, $Asset, [int]$X, [int]$Y, [int]$CellSize) {
    if ($null -eq $Asset) { return }
    $image = Image-For $Asset
    $sourceX = (($X * $CellSize) % $image.Width + $image.Width) % $image.Width
    $sourceY = (($Y * $CellSize) % $image.Height + $image.Height) % $image.Height
    $sourceWidth = [Math]::Min($CellSize, $image.Width - $sourceX)
    $sourceHeight = [Math]::Min($CellSize, $image.Height - $sourceY)
    $destination = [Drawing.Rectangle]::new($X * $CellSize, $Y * $CellSize, $CellSize, $CellSize)
    $Graphics.DrawImage($image, $destination, $sourceX, $sourceY, $sourceWidth, $sourceHeight, [Drawing.GraphicsUnit]::Pixel)
    Record-Usage $Usage $Asset
}
function Draw-Stamp($Graphics, $Usage, $Asset, [float]$CenterX, [float]$CenterY, [float]$WidthCells, [float]$HeightCells, [int]$CellSize, [int]$Rotation=0) {
    if ($null -eq $Asset) { return }
    $image = Image-For $Asset
    $width = [single]($WidthCells * $CellSize); $height = [single]($HeightCells * $CellSize)
    $state = $Graphics.Save()
    try {
        $Graphics.TranslateTransform([single]($CenterX * $CellSize), [single]($CenterY * $CellSize))
        if (($Rotation % 4) -ne 0) { $Graphics.RotateTransform([single](90 * ($Rotation % 4))) }
        $Graphics.DrawImage($image, [Drawing.RectangleF]::new(-$width / 2, -$height / 2, $width, $height))
    } finally { $Graphics.Restore($state) }
    Record-Usage $Usage $Asset
}
function Cell-At($Map, [int]$X, [int]$Y) {
    if ($X -lt 0 -or $Y -lt 0 -or $X -ge [int]$Map.width -or $Y -ge [int]$Map.height) { return $null }
    return $Map.cells[$Y][$X]
}
function Way-Rotation($Map, [int]$X, [int]$Y, [int]$Way) {
    $left=Cell-At $Map ($X-1) $Y; $right=Cell-At $Map ($X+1) $Y; $up=Cell-At $Map $X ($Y-1); $down=Cell-At $Map $X ($Y+1)
    $horizontal = (($null-ne$left -and [int]$left.way -eq $Way) -or ($null-ne$right -and [int]$right.way -eq $Way))
    $vertical = (($null-ne$up -and [int]$up.way -eq $Way) -or ($null-ne$down -and [int]$down.way -eq $Way))
    if ($vertical -and -not $horizontal) { return 1 }
    return 0
}
function Material-Role([int]$Kind, [int]$Scope, [string]$Preset, [int]$Seed, [int]$X, [int]$Y) {
    switch ($Kind) {
        1 { if ((Stable-Index $Seed $X $Y 71 5) -eq 0) { return "water_deep" }; return "water_shallow" }
        2 { return "forest_floor" }
        3 { return "gravel" }
        4 { return "earth" }
        5 { return "earth" }
        6 { if ($Preset -eq "City") { return "dressed_stone_floor" }; return "grass_temperate" }
        7 { if ($Preset -eq "City") { return "dressed_stone_floor" }; return "grass_temperate" }
        8 { return "dressed_stone_floor" }
        9 { return "dressed_stone_floor" }
        10 { return "dressed_stone_floor" }
        11 { return "cave_floor" }
        12 { return "dressed_stone_floor" }
        13 { return "mud" }
        15 { return "earth" }
        16 { return "gravel" }
        18 { return "coastal_sand" }
        19 { return "mud" }
        20 { return "cave_floor" }
        22 { return "cave_floor" }
        23 { return "gravel" }
        24 { return "grass_temperate" }
        25 { return "cave_floor" }
        26 { return "water_shallow" }
        default { if ($Scope -eq 2) { return "cave_floor" }; return "grass_temperate" }
    }
}

$mapFiles = @(Get-ChildItem -LiteralPath $inputPath -Filter "*.json" | Where-Object { $_.Name -notin @("gallery_manifest.json") })
$rendered = [Collections.Generic.List[object]]::new()
foreach ($mapFile in $mapFiles) {
    $map = Get-Content -Raw -LiteralPath $mapFile.FullName | ConvertFrom-Json
    if ($null -eq $map.cells -or [int]$map.width -le 0 -or [int]$map.height -le 0) { continue }
    $usage = @{}; $seed=[int]$map.seed; $scope=[int]$map.scope; $preset=[string]$map.preset
    $bitmap = [Drawing.Bitmap]::new([int]$map.width*$PixelsPerCell,[int]$map.height*$PixelsPerCell,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.Clear([Drawing.Color]::FromArgb(255,34,31,25))
        $graphics.CompositingQuality=[Drawing.Drawing2D.CompositingQuality]::HighQuality
        $graphics.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode=[Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        for($y=0;$y-lt[int]$map.height;$y++) { for($x=0;$x-lt[int]$map.width;$x++) {
            $cell=$map.cells[$y][$x]; $role=Material-Role ([int]$cell.kind) $scope $preset $seed $x $y
            $asset=Asset-For $role $seed ([int]($x/16)) ([int]($y/16)) 1
            Draw-Material $graphics $usage $asset $x $y $PixelsPerCell
        }}
        for($y=0;$y-lt[int]$map.height;$y++) { for($x=0;$x-lt[int]$map.width;$x++) {
            $cell=$map.cells[$y][$x]; $way=[int]$cell.way
            if($way-ne0) {
                $role=switch($way){1{"path"}2{"road_dirt"}3{"road_stone"}4{"river"}5{"canal"}6{"road_stone"}7{"path"}default{"path"}}
                $asset=Asset-For $role $seed $x $y 17
                Draw-Stamp $graphics $usage $asset ($x+0.5) ($y+0.5) 1.18 1.18 $PixelsPerCell (Way-Rotation $map $x $y $way)
            }
        }}
        for($y=0;$y-lt[int]$map.height;$y++) { for($x=0;$x-lt[int]$map.width;$x++) {
            $cell=$map.cells[$y][$x]; $kind=[int]$cell.kind; $choice=Stable-Index $seed $x $y 31 100
            if($kind-eq2 -and $choice-lt28) {
                $role=@("deciduous_group","conifer_group","mixed_tree_group","bushes")[(Stable-Index $seed $x $y 32 4)]
                Draw-Stamp $graphics $usage (Asset-For $role $seed $x $y 33) ($x+0.5) ($y+0.5) 2.1 2.1 $PixelsPerCell (Stable-Index $seed $x $y 34 4)
            } elseif($kind-eq3 -and $choice-lt34) {
                $role=if((Stable-Index $seed $x $y 35 4)-eq0){"connected_ridge"}else{"peak"}
                Draw-Stamp $graphics $usage (Asset-For $role $seed $x $y 36) ($x+0.5) ($y+0.5) 3.0 3.0 $PixelsPerCell 0
            } elseif($kind-eq4 -and $choice-lt18) {
                Draw-Stamp $graphics $usage (Asset-For "foothill" $seed $x $y 37) ($x+0.5) ($y+0.5) 2.4 2.4 $PixelsPerCell 0
            } elseif($kind-in@(6,7)) {
                if($scope-eq0) {
                    $role=if($kind-eq7){"capital"}elseif((Stable-Index $seed $x $y 38 3)-eq0){"city"}else{"town"}
                    Draw-Stamp $graphics $usage (Asset-For $role $seed $x $y 39) ($x+0.5) ($y+0.5) 1.45 1.45 $PixelsPerCell 0
                } elseif($choice-lt55) {
                    $role=if($preset-eq"City"){"town_service"}else{"rural"}
                    Draw-Stamp $graphics $usage (Asset-For $role $seed $x $y 40) ($x+0.5) ($y+0.5) 2.0 2.0 $PixelsPerCell (Stable-Index $seed $x $y 41 4)
                }
            } elseif($kind-eq9) {
                Draw-Stamp $graphics $usage (Asset-For "dungeon_architecture" $seed $x $y 42) ($x+0.5) ($y+0.5) 1.15 1.15 $PixelsPerCell ([int]$cell.rotation)
            } elseif($kind-eq10) {
                Draw-Stamp $graphics $usage (Asset-For "door" $seed $x $y 43) ($x+0.5) ($y+0.5) 0.82 0.82 $PixelsPerCell ([int]$cell.rotation)
            } elseif($kind-eq11 -and $choice-lt16) {
                Draw-Stamp $graphics $usage (Asset-For "cave_natural" $seed $x $y 44) ($x+0.5) ($y+0.5) 1.25 1.25 $PixelsPerCell (Stable-Index $seed $x $y 45 4)
            } elseif($kind-eq13) {
                Draw-Stamp $graphics $usage (Asset-For "hazard" $seed $x $y 46) ($x+0.5) ($y+0.5) 0.72 0.72 $PixelsPerCell 0
            } elseif($kind-eq14) {
                Draw-Stamp $graphics $usage (Asset-For "ruin" $seed $x $y 47) ($x+0.5) ($y+0.5) 2.0 2.0 $PixelsPerCell (Stable-Index $seed $x $y 48 4)
            } elseif($kind-eq23) {
                Draw-Stamp $graphics $usage (Asset-For "rubble" $seed $x $y 49) ($x+0.5) ($y+0.5) 1.0 1.0 $PixelsPerCell (Stable-Index $seed $x $y 50 4)
            } elseif($kind-eq24) {
                Draw-Stamp $graphics $usage (Asset-For "well_log_cover" $seed $x $y 51) ($x+0.5) ($y+0.5) 1.0 1.0 $PixelsPerCell (Stable-Index $seed $x $y 52 4)
            } elseif($kind-eq26) {
                Draw-Stamp $graphics $usage (Asset-For "bridge_wood_heavy" $seed $x $y 53) ($x+0.5) ($y+0.5) 2.15 1.15 $PixelsPerCell ([int]$cell.rotation)
            }
            if($scope-ne0 -and $kind-in@(0,2,4,15,16) -and (Stable-Index $seed $x $y 54 100)-lt3) {
                Draw-Stamp $graphics $usage (Asset-For "ground_detail_decal" $seed $x $y 55) ($x+0.5) ($y+0.5) 0.8 0.8 $PixelsPerCell (Stable-Index $seed $x $y 56 4)
            }
        }}
        if($scope-eq2 -or $mapFile.BaseName -match '^1[5-8]_') {
            $pen=[Drawing.Pen]::new([Drawing.Color]::FromArgb(48,238,226,196),1)
            try { for($x=0;$x-le[int]$map.width;$x++){$graphics.DrawLine($pen,$x*$PixelsPerCell,0,$x*$PixelsPerCell,[int]$map.height*$PixelsPerCell)};for($y=0;$y-le[int]$map.height;$y++){$graphics.DrawLine($pen,0,$y*$PixelsPerCell,[int]$map.width*$PixelsPerCell,$y*$PixelsPerCell)} } finally {$pen.Dispose()}
        }
        $target=Join-Path $outputPath ($mapFile.BaseName+".png")
        $bitmap.Save($target,[Drawing.Imaging.ImageFormat]::Png)
        $usageRecords=@($usage.GetEnumerator()|Sort-Object Name|ForEach-Object{[ordered]@{asset_id=$_.Name;placements=$_.Value}})
        $placementCount=0; foreach($record in $usageRecords){$placementCount += [int]$record.placements}
        [ordered]@{schema_version=1;snapshot_id=$snapshot.snapshot_id;map=$mapFile.Name;seed=$seed;unique_asset_count=$usageRecords.Count;placements=$placementCount;assets=$usageRecords}|ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $outputPath ($mapFile.BaseName+".asset-usage.json")) -Encoding utf8
        $rendered.Add([pscustomobject]@{map=$mapFile.BaseName;png=$target;unique_assets=$usageRecords.Count})
    } finally { $graphics.Dispose(); $bitmap.Dispose() }
}
foreach($image in @($imageCache.Values)){ $image.Dispose() }
Write-Host "Rendered $($rendered.Count) maps with FantasyNeutral V3 production assets."
$rendered | Format-Table -AutoSize

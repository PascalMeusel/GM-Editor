param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$OutputPath = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2/review/contextual_candidates/material_study_v101_contact_sheet.png"
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

function Resolve-RepoPath([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }
    return Join-Path (Resolve-Path ".").Path $Path
}

function New-BaseTile($SourceBitmap, [int]$X, [int]$Y) {
    $bitmap = [System.Drawing.Bitmap]::new(128, 128, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.DrawImage(
        $SourceBitmap,
        [System.Drawing.Rectangle]::new(0, 0, 128, 128),
        [System.Drawing.Rectangle]::new($X, $Y, 128, 128),
        [System.Drawing.GraphicsUnit]::Pixel)
    $graphics.Dispose()
    return $bitmap
}

function Get-Caption([string]$RelativePath) {
    if ($RelativePath -match "__(mask[0-9]{3})__") {
        return $Matches[1]
    }
    $name = [System.IO.Path]::GetFileNameWithoutExtension($RelativePath)
    $name = $name -replace "^fantasy_neutral__local__structure__guard_tower__", ""
    $name = $name -replace "__v[0-9]+$", ""
    return $name
}

$root = Resolve-RepoPath $SourceRoot
$sourcePath = Join-Path $root "ai_source/material_study_roads_rivers_20260531.png"
$outputFullPath = Resolve-RepoPath $OutputPath
$outputDirectory = Split-Path -Parent $outputFullPath
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null

$source = [System.Drawing.Bitmap]::FromFile($sourcePath)

$bases = @{
    grass = New-BaseTile $source 560 520
    dirt = New-BaseTile $source 160 530
    stone = New-BaseTile $source 940 210
    water = New-BaseTile $source 900 870
}

$rows = @(
    @{
        label = "dirt road / grass"
        base = "grass"
        files = @(
            "way_segment/way_dirt_road_on_grass_open/fantasy_neutral__world__way_segment__way_dirt_road_on_grass_open__mask017__v101.png",
            "way_segment/way_dirt_road_on_grass_open/fantasy_neutral__world__way_segment__way_dirt_road_on_grass_open__mask034__v102.png",
            "way_segment/way_dirt_road_on_grass_open/fantasy_neutral__world__way_segment__way_dirt_road_on_grass_open__mask085__v103.png",
            "way_segment/way_dirt_road_on_grass_open/fantasy_neutral__world__way_segment__way_dirt_road_on_grass_open__mask255__v104.png"
        )
    },
    @{
        label = "dirt road / dirt"
        base = "dirt"
        files = @(
            "way_segment/way_dirt_road_on_dirt/fantasy_neutral__world__way_segment__way_dirt_road_on_dirt__mask017__v101.png",
            "way_segment/way_dirt_road_on_dirt/fantasy_neutral__world__way_segment__way_dirt_road_on_dirt__mask034__v102.png",
            "way_segment/way_dirt_road_on_dirt/fantasy_neutral__world__way_segment__way_dirt_road_on_dirt__mask085__v103.png",
            "way_segment/way_dirt_road_on_dirt/fantasy_neutral__world__way_segment__way_dirt_road_on_dirt__mask255__v104.png"
        )
    },
    @{
        label = "stone road / stone"
        base = "stone"
        files = @(
            "way_segment/way_stone_road_on_stone_paved/fantasy_neutral__city__way_segment__way_stone_road_on_stone_paved__mask017__v101.png",
            "way_segment/way_stone_road_on_stone_paved/fantasy_neutral__city__way_segment__way_stone_road_on_stone_paved__mask034__v102.png",
            "way_segment/way_stone_road_on_stone_paved/fantasy_neutral__city__way_segment__way_stone_road_on_stone_paved__mask085__v103.png",
            "way_segment/way_stone_road_on_stone_paved/fantasy_neutral__city__way_segment__way_stone_road_on_stone_paved__mask255__v104.png"
        )
    },
    @{
        label = "trail / grass"
        base = "grass"
        files = @(
            "way_segment/way_trail_narrow_on_grass_open/fantasy_neutral__world__way_segment__way_trail_narrow_on_grass_open__mask017__v101.png",
            "way_segment/way_trail_narrow_on_grass_open/fantasy_neutral__world__way_segment__way_trail_narrow_on_grass_open__mask034__v102.png",
            "way_segment/way_trail_narrow_on_grass_open/fantasy_neutral__world__way_segment__way_trail_narrow_on_grass_open__mask085__v101.png",
            "way_segment/way_trail_narrow_on_grass_open/fantasy_neutral__world__way_segment__way_trail_narrow_on_grass_open__mask255__v102.png"
        )
    },
    @{
        label = "trail / dirt"
        base = "dirt"
        files = @(
            "way_segment/way_trail_narrow_on_dirt/fantasy_neutral__world__way_segment__way_trail_narrow_on_dirt__mask017__v101.png",
            "way_segment/way_trail_narrow_on_dirt/fantasy_neutral__world__way_segment__way_trail_narrow_on_dirt__mask034__v102.png",
            "way_segment/way_trail_narrow_on_dirt/fantasy_neutral__world__way_segment__way_trail_narrow_on_dirt__mask085__v101.png",
            "way_segment/way_trail_narrow_on_dirt/fantasy_neutral__world__way_segment__way_trail_narrow_on_dirt__mask255__v102.png"
        )
    },
    @{
        label = "river / water"
        base = "water"
        files = @(
            "way_segment/way_river_stream_on_water_shallow/fantasy_neutral__world__way_segment__way_river_stream_on_water_shallow__mask017__v101.png",
            "way_segment/way_river_stream_on_water_shallow/fantasy_neutral__world__way_segment__way_river_stream_on_water_shallow__mask034__v102.png",
            "way_segment/way_river_stream_on_water_shallow/fantasy_neutral__world__way_segment__way_river_stream_on_water_shallow__mask085__v103.png",
            "way_segment/way_river_stream_on_water_shallow/fantasy_neutral__world__way_segment__way_river_stream_on_water_shallow__mask255__v104.png"
        )
    },
    @{
        label = "guard tower contexts"
        base = "grass"
        files = @(
            "structure/guard_tower/fantasy_neutral__local__structure__guard_tower__ground_only__v101.png",
            "structure/guard_tower/fantasy_neutral__local__structure__guard_tower__entrance_n__v101.png",
            "structure/guard_tower/fantasy_neutral__local__structure__guard_tower__entrance_stone_s__v101.png",
            "structure/guard_tower/fantasy_neutral__local__structure__guard_tower__bridgehead__v101.png",
            "structure/guard_tower/fantasy_neutral__local__structure__guard_tower__cave_threshold__v101.png",
            "structure/guard_tower/fantasy_neutral__local__structure__guard_tower__dungeon_threshold__v101.png"
        )
    }
)

$cell = 128
$gap = 18
$labelWidth = 185
$top = 48
$rowHeight = 166
$columns = 6
$width = $labelWidth + (($cell + $gap) * $columns) + 28
$height = $top + ($rowHeight * $rows.Count) + 24
$sheet = [System.Drawing.Bitmap]::new($width, $height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [System.Drawing.Graphics]::FromImage($sheet)
$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
$graphics.Clear([System.Drawing.Color]::FromArgb(255, 32, 34, 36))

$titleFont = [System.Drawing.Font]::new("Segoe UI", 14, [System.Drawing.FontStyle]::Bold)
$labelFont = [System.Drawing.Font]::new("Segoe UI", 10, [System.Drawing.FontStyle]::Bold)
$smallFont = [System.Drawing.Font]::new("Segoe UI", 8, [System.Drawing.FontStyle]::Regular)
$mutedBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(215, 220, 220, 220))
$borderPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(120, 255, 255, 255), 1)

$graphics.DrawString("FantasyNeutral v2 material-study v101 candidate art preview", $titleFont, [System.Drawing.Brushes]::White, 18, 14)

for ($rowIndex = 0; $rowIndex -lt $rows.Count; $rowIndex++) {
    $row = $rows[$rowIndex]
    $y = $top + ($rowIndex * $rowHeight)
    $graphics.DrawString($row.label, $labelFont, [System.Drawing.Brushes]::White, 18, $y + 46)
    $graphics.DrawString("candidate-only", $smallFont, $mutedBrush, 18, $y + 68)

    for ($column = 0; $column -lt $row.files.Count; $column++) {
        $x = $labelWidth + ($column * ($cell + $gap))
        $graphics.DrawImage($bases[$row.base], $x, $y, $cell, $cell)

        $tilePath = Join-Path $root $row.files[$column]
        if (Test-Path $tilePath) {
            $tile = [System.Drawing.Bitmap]::FromFile($tilePath)
            $graphics.DrawImage($tile, $x, $y, $cell, $cell)
            $tile.Dispose()
        }

        $graphics.DrawRectangle($borderPen, $x, $y, $cell, $cell)
        $graphics.DrawString((Get-Caption $row.files[$column]), $smallFont, $mutedBrush, $x, $y + $cell + 4)
    }
}

$sheet.Save($outputFullPath, [System.Drawing.Imaging.ImageFormat]::Png)

$graphics.Dispose()
$sheet.Dispose()
$source.Dispose()
$titleFont.Dispose()
$labelFont.Dispose()
$smallFont.Dispose()
$mutedBrush.Dispose()
$borderPen.Dispose()
foreach ($base in $bases.Values) {
    $base.Dispose()
}

Write-Host "Material-study contact sheet wrote $outputFullPath"

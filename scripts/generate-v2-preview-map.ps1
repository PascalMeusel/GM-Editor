param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$OutputDirectory = "",
    [int]$TileSize = 48,
    [string]$MapName = "first_vertical_slice_wilderness_preview",
    [string]$BaseIdContains = "",
    [int]$BaseVariantMin = 0,
    [int]$BaseVariantMax = 0,
    [switch]$BaseOnly
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

$resolvedSourceRoot = Resolve-TilesetPath $SourceRoot
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $resolvedSourceRoot "review/map_stress_tests"
}
$resolvedOutputDirectory = Resolve-TilesetPath $OutputDirectory
New-Item -ItemType Directory -Force -Path $resolvedOutputDirectory | Out-Null

$requiredFamilies = @("grass_open", "snow", "water_shallow", "dirt")
$fallbackFamily = "grass_open"
$width = 18
$height = 12

$bundle = Get-TilesetV2ManifestBundle $resolvedSourceRoot
$tileAssets = @(Get-TilesetManifestAssets $bundle.Tiles)
$transitionAssets = @(Get-TilesetManifestAssets $bundle.Transitions)

function Get-AvailableTerrainAssets([string]$Family) {
    $allowedClasses = if ($BaseOnly) { @("base") } else { @("base", "same_auto") }
    $assets = @($tileAssets | Where-Object {
        (Get-TilesetProperty $_ "owner" "") -eq $Family -and
        (Get-TilesetProperty $_ "asset_class" "") -in $allowedClasses -and
        (Get-TilesetProperty $_ "status" "") -in @("candidate", "accepted")
    })
    if (-not [string]::IsNullOrWhiteSpace($BaseIdContains)) {
        $assets = @($assets | Where-Object { (Get-TilesetProperty $_ "id" "") -like "*$BaseIdContains*" })
    }
    if ($BaseVariantMin -gt 0) {
        $assets = @($assets | Where-Object { [int](Get-TilesetProperty $_ "variant" 0) -ge $BaseVariantMin })
    }
    if ($BaseVariantMax -gt 0) {
        $assets = @($assets | Where-Object { [int](Get-TilesetProperty $_ "variant" 0) -le $BaseVariantMax })
    }
    return $assets
}

function Test-FamilyAvailable([string]$Family) {
    return ((Get-AvailableTerrainAssets $Family).Count -gt 0)
}

function Get-SameFamilyMaskForGrid($Grid, [int]$X, [int]$Y, [string]$Family) {
    $mask = 0
    if ($Y -gt 0 -and $Grid[$Y - 1][$X] -eq $Family) { $mask = $mask -bor 1 }
    if ($X -lt ($Grid[0].Count - 1) -and $Grid[$Y][$X + 1] -eq $Family) { $mask = $mask -bor 2 }
    if ($Y -lt ($Grid.Count - 1) -and $Grid[$Y + 1][$X] -eq $Family) { $mask = $mask -bor 4 }
    if ($X -gt 0 -and $Grid[$Y][$X - 1] -eq $Family) { $mask = $mask -bor 8 }
    return $mask
}

function Get-NeighborMaskForGrid($Grid, [int]$X, [int]$Y, [string]$Neighbor) {
    $mask = 0
    if ($Y -gt 0 -and $Grid[$Y - 1][$X] -eq $Neighbor) { $mask = $mask -bor 1 }
    if ($X -lt ($Grid[0].Count - 1) -and $Grid[$Y][$X + 1] -eq $Neighbor) { $mask = $mask -bor 2 }
    if ($Y -lt ($Grid.Count - 1) -and $Grid[$Y + 1][$X] -eq $Neighbor) { $mask = $mask -bor 4 }
    if ($X -gt 0 -and $Grid[$Y][$X - 1] -eq $Neighbor) { $mask = $mask -bor 8 }
    return $mask
}

function Add-RectToGrid($Grid, [string]$Family, [int]$Left, [int]$Top, [int]$Right, [int]$Bottom) {
    for ($y = $Top; $y -le $Bottom; ++$y) {
        for ($x = $Left; $x -le $Right; ++$x) {
            if ($y -ge 0 -and $y -lt $Grid.Count -and $x -ge 0 -and $x -lt $Grid[$y].Count) {
                $Grid[$y][$x] = $Family
            }
        }
    }
}

function Add-CellsToGrid($Grid, [string]$Family, [object[]]$Cells) {
    foreach ($cell in $Cells) {
        $x = [int]$cell[0]
        $y = [int]$cell[1]
        if ($y -ge 0 -and $y -lt $Grid.Count -and $x -ge 0 -and $x -lt $Grid[$y].Count) {
            $Grid[$y][$x] = $Family
        }
    }
}

function Draw-MissingFamilyOverlay($Graphics, [Drawing.Rectangle]$Destination, [string]$Family) {
    $color = switch ($Family) {
        "water_shallow" { [Drawing.Color]::FromArgb(132, 20, 92, 130) }
        "dirt" { [Drawing.Color]::FromArgb(132, 124, 76, 34) }
        default { [Drawing.Color]::FromArgb(132, 130, 40, 80) }
    }

    $brush = [Drawing.SolidBrush]::new($color)
    $pen = [Drawing.Pen]::new([Drawing.Color]::FromArgb(220, 255, 236, 132), 2)
    $hatchPen = [Drawing.Pen]::new([Drawing.Color]::FromArgb(150, 255, 236, 132), 1)
    try {
        $Graphics.FillRectangle($brush, $Destination)
        $Graphics.DrawRectangle($pen, $Destination.X, $Destination.Y, $Destination.Width - 1, $Destination.Height - 1)
        for ($offset = -$Destination.Height; $offset -lt $Destination.Width; $offset += 10) {
            $x1 = $Destination.X + $offset
            $y1 = $Destination.Y + $Destination.Height
            $x2 = $Destination.X + $offset + $Destination.Height
            $y2 = $Destination.Y
            $Graphics.DrawLine($hatchPen, $x1, $y1, $x2, $y2)
        }
    }
    finally {
        $brush.Dispose()
        $pen.Dispose()
        $hatchPen.Dispose()
    }
}

$intendedGrid = @()
for ($y = 0; $y -lt $height; ++$y) {
    $row = @()
    for ($x = 0; $x -lt $width; ++$x) {
        $row += "grass_open"
    }
    $intendedGrid += ,$row
}

# Repeated snowfield with a grass pocket and jagged grass/snow contacts.
Add-RectToGrid $intendedGrid "snow" 11 0 17 3
Add-RectToGrid $intendedGrid "snow" 13 4 17 5
Add-CellsToGrid $intendedGrid "grass_open" @((13,1), (14,1), (14,2), (16,4))

# Snow islands inside grass for same-family and cross-family repeat review.
Add-CellsToGrid $intendedGrid "snow" @((3,2), (4,2), (3,3), (4,3), (7,1), (8,1), (8,2), (6,8), (7,8), (6,9))

# Planned shallow-water coast and island shapes. These render as missing placeholders until water candidates land.
Add-CellsToGrid $intendedGrid "water_shallow" @(
    (0,7), (1,7), (2,7), (3,7),
    (0,8), (1,8), (2,8), (3,8), (4,8),
    (0,9), (1,9), (4,9),
    (0,10), (1,10), (2,10), (3,10), (4,10),
    (0,11), (1,11), (2,11), (3,11),
    (13,7), (14,7), (15,7), (13,8), (15,8), (13,9), (14,9), (15,9)
)

# Planned dirt clearings and path fragments. These render as missing placeholders until dirt candidates land.
Add-CellsToGrid $intendedGrid "dirt" @(
    (7,5), (8,5), (9,5),
    (6,6), (7,6), (8,6), (9,6), (10,6),
    (6,7), (7,7), (8,7), (9,7),
    (9,8), (10,8), (11,8),
    (12,10), (13,10), (14,10), (15,10),
    (13,11), (14,11)
)

$familyAvailability = @{}
foreach ($family in $requiredFamilies) {
    $familyAvailability[$family] = Test-FamilyAvailable $family
}

$renderedGrid = @()
$missingCells = @()
for ($y = 0; $y -lt $height; ++$y) {
    $row = @()
    for ($x = 0; $x -lt $width; ++$x) {
        $family = $intendedGrid[$y][$x]
        if ($familyAvailability[$family]) {
            $row += $family
        }
        else {
            $row += $fallbackFamily
            $missingCells += [pscustomobject]@{
                x = $x
                y = $y
                intended_family = $family
                rendered_as = $fallbackFamily
            }
        }
    }
    $renderedGrid += ,$row
}

$tileUsage = @{}
$transitionUsage = @{}
$missingTransitionMasks = @{}

$review = New-TilesetReviewBitmap ($width * $TileSize) ($height * $TileSize)
try {
    for ($y = 0; $y -lt $height; ++$y) {
        for ($x = 0; $x -lt $width; ++$x) {
            $family = $renderedGrid[$y][$x]
            $mask = Get-SameFamilyMaskForGrid $renderedGrid $x $y $family
            $asset = Select-TilesetAsset $tileAssets $family @("same_auto") $mask $x $y 67
            if ($null -eq $asset) {
                $asset = Select-TilesetAsset $tileAssets $family @("base") $null $x $y 71
            }
            $dest = [Drawing.Rectangle]::new($x * $TileSize, $y * $TileSize, $TileSize, $TileSize)
            Draw-TilesetAsset $review.Graphics $asset $dest

            $assetId = Get-TilesetProperty $asset "id" "missing_asset"
            if (-not $tileUsage.ContainsKey($assetId)) { $tileUsage[$assetId] = 0 }
            $tileUsage[$assetId] += 1
        }
    }

    for ($y = 0; $y -lt $height; ++$y) {
        for ($x = 0; $x -lt $width; ++$x) {
            $owner = $renderedGrid[$y][$x]
            $neighborFamilies = @($requiredFamilies | Where-Object { $_ -ne $owner })
            foreach ($neighbor in $neighborFamilies) {
                $mask = Get-NeighborMaskForGrid $renderedGrid $x $y $neighbor
                if ($mask -eq 0) {
                    continue
                }
                $transition = Select-TilesetTransitionAsset $transitionAssets $owner $neighbor $mask $x $y 79
                if ($null -eq $transition) {
                    $missingKey = "$owner->$neighbor mask$($mask.ToString("00"))"
                    if (-not $missingTransitionMasks.ContainsKey($missingKey)) {
                        $missingTransitionMasks[$missingKey] = [pscustomobject]@{
                            owner = $owner
                            neighbor = $neighbor
                            mask = $mask
                            contact_cell_count = 0
                        }
                    }
                    $missingTransitionMasks[$missingKey].contact_cell_count += 1
                    continue
                }
                $dest = [Drawing.Rectangle]::new($x * $TileSize, $y * $TileSize, $TileSize, $TileSize)
                Draw-TilesetAsset $review.Graphics $transition $dest

                $transitionId = Get-TilesetProperty $transition "id" "missing_transition_id"
                if (-not $transitionUsage.ContainsKey($transitionId)) { $transitionUsage[$transitionId] = 0 }
                $transitionUsage[$transitionId] += 1
            }
        }
    }

    foreach ($cell in $missingCells) {
        $dest = [Drawing.Rectangle]::new($cell.x * $TileSize, $cell.y * $TileSize, $TileSize, $TileSize)
        Draw-MissingFamilyOverlay $review.Graphics $dest $cell.intended_family
    }

    $pngPath = Join-Path $resolvedOutputDirectory "$MapName.png"
    $review.Bitmap.Save($pngPath, [Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $review.Graphics.Dispose()
    $review.Bitmap.Dispose()
}

$familySummary = @()
foreach ($family in $requiredFamilies) {
    $assets = Get-AvailableTerrainAssets $family
    $baseCount = @($assets | Where-Object { (Get-TilesetProperty $_ "asset_class" "") -eq "base" }).Count
    $sameAutoCount = @($assets | Where-Object { (Get-TilesetProperty $_ "asset_class" "") -eq "same_auto" }).Count
    $intendedCount = @($missingCells | Where-Object { $_.intended_family -eq $family }).Count
    if ($familyAvailability[$family]) {
        $intendedCount = 0
        for ($y = 0; $y -lt $height; ++$y) {
            for ($x = 0; $x -lt $width; ++$x) {
                if ($intendedGrid[$y][$x] -eq $family) { $intendedCount += 1 }
            }
        }
    }
    $familySummary += [pscustomobject]@{
        family = $family
        available = [bool]$familyAvailability[$family]
        candidate_or_accepted_base_count = $baseCount
        candidate_or_accepted_same_auto_count = $sameAutoCount
        intended_cell_count = $intendedCount
        rendering = if ($familyAvailability[$family]) { "candidate_tiles" } else { "$fallbackFamily fallback plus missing-family overlay" }
    }
}

$usedTiles = @(
    foreach ($key in ($tileUsage.Keys | Sort-Object)) {
        [pscustomobject]@{
            id = $key
            draw_count = $tileUsage[$key]
        }
    }
)

$usedTransitions = @(
    foreach ($key in ($transitionUsage.Keys | Sort-Object)) {
        [pscustomobject]@{
            id = $key
            draw_count = $transitionUsage[$key]
        }
    }
)

$missingTransitionSummary = @(
    foreach ($key in ($missingTransitionMasks.Keys | Sort-Object)) {
        $missingTransitionMasks[$key]
    }
)

$intendedRows = @()
$renderedRows = @()
for ($y = 0; $y -lt $height; ++$y) {
    $intendedRows += ($intendedGrid[$y] -join ",")
    $renderedRows += ($renderedGrid[$y] -join ",")
}

$visualConcerns = @()
foreach ($family in $requiredFamilies) {
    if (-not $familyAvailability[$family]) {
        $visualConcerns += "$family has no candidate or accepted terrain assets in tiles.json; intended cells are rendered as $fallbackFamily with a missing-family overlay."
    }
}
if ($usedTransitions.Count -eq 0) {
    $visualConcerns += "transition_pairs.json has no drawable transition_edge candidates for the contacts in this preview; all terrain borders are hard base/same_auto adjacency."
}
elseif ($missingTransitionSummary.Count -eq 0) {
    $visualConcerns += "All transition masks requested by this preview were available; visual acceptance still depends on whether the overlays read as natural feathering instead of repeated border marks."
}
$visualConcerns += "This preview is candidate-only; it supports comparative QA and does not promote assets to accepted runtime status."

$sidecar = [pscustomobject]@{
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    source_root = ConvertTo-TilesetRelativePath $resolvedSourceRoot
    map_name = $MapName
    output_png = ConvertTo-TilesetRelativePath $pngPath
    tile_size = $TileSize
    width = $width
    height = $height
    required_families = $requiredFamilies
    family_availability = $familySummary
    base_filters = [pscustomobject]@{
        base_id_contains = $BaseIdContains
        base_variant_min = $BaseVariantMin
        base_variant_max = $BaseVariantMax
        base_only = [bool]$BaseOnly
    }
    designed_features = @(
        "large repeated grass_open field",
        "large repeated snow field",
        "snow islands inside grass",
        "grass pocket inside snow",
        "planned water_shallow coast, pond, and islands",
        "planned dirt clearings and path-like patches"
    )
    missing_family_cells = $missingCells
    intended_family_rows = $intendedRows
    rendered_family_rows = $renderedRows
    tile_usage = $usedTiles
    transition_overlay_usage = $usedTransitions
    missing_transition_masks = $missingTransitionSummary
    visual_concerns = $visualConcerns
}

$jsonPath = Join-Path $resolvedOutputDirectory "$MapName.json"
$sidecar | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $jsonPath

Write-Host "V2 preview map wrote $pngPath"
Write-Host "V2 preview map sidecar wrote $jsonPath"

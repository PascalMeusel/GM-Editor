param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$OutputDirectory = "",
    [int]$TileSize = 40,
    [int]$Width = 12,
    [int]$Height = 8
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

$resolvedSourceRoot = Resolve-TilesetPath $SourceRoot
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $resolvedSourceRoot "review/map_stress_tests"
}
$resolvedOutputDirectory = Resolve-TilesetPath $OutputDirectory
New-Item -ItemType Directory -Force -Path $resolvedOutputDirectory | Out-Null

$bundle = Get-TilesetV2ManifestBundle $resolvedSourceRoot
$tileAssets = @(Get-TilesetManifestAssets $bundle.Tiles)
$transitionAssets = @(Get-TilesetManifestAssets $bundle.Transitions)
$orderedPairs = ConvertTo-TilesetArray (Get-TilesetProperty $bundle.Transitions "ordered_pairs" @())
$results = @()

function Get-NeighborMaskForGrid($Grid, [int]$X, [int]$Y, [string]$Neighbor) {
    $mask = 0
    if ($Y -gt 0 -and $Grid[$Y - 1][$X] -eq $Neighbor) { $mask = $mask -bor 1 }
    if ($X -lt ($Grid[0].Count - 1) -and $Grid[$Y][$X + 1] -eq $Neighbor) { $mask = $mask -bor 2 }
    if ($Y -lt ($Grid.Count - 1) -and $Grid[$Y + 1][$X] -eq $Neighbor) { $mask = $mask -bor 4 }
    if ($X -gt 0 -and $Grid[$Y][$X - 1] -eq $Neighbor) { $mask = $mask -bor 8 }
    return $mask
}

foreach ($pair in $orderedPairs) {
    $owner = Get-TilesetProperty $pair "owner" ""
    $neighbor = Get-TilesetProperty $pair "neighbor" ""
    if (-not $owner -or -not $neighbor) {
        continue
    }

    $ownerBase = Select-TilesetAsset $tileAssets $owner @("base", "same_auto") 15 0 0 51
    if ($null -eq $ownerBase) { $ownerBase = Select-TilesetAsset $tileAssets $owner @("base") $null 0 0 51 }
    $neighborBase = Select-TilesetAsset $tileAssets $neighbor @("base", "same_auto") 15 1 0 53
    if ($null -eq $neighborBase) { $neighborBase = Select-TilesetAsset $tileAssets $neighbor @("base") $null 1 0 53 }

    $pairTransitions = @($transitionAssets | Where-Object {
        (Get-TilesetProperty $_ "asset_class" "") -eq "transition_edge" -and
        (Get-TilesetProperty $_ "owner" "") -eq $owner -and
        (Get-TilesetProperty $_ "neighbor" "") -eq $neighbor -and
        (Get-TilesetProperty $_ "status" "") -in @("candidate", "accepted")
    })
    if ($null -eq $ownerBase -or $null -eq $neighborBase -or $pairTransitions.Count -eq 0) {
        $results += [pscustomobject]@{
            owner = $owner
            neighbor = $neighbor
            status = "skipped"
            reason = "missing_owner_base_neighbor_base_or_transition_assets"
        }
        continue
    }

    $grid = @()
    for ($y = 0; $y -lt $Height; ++$y) {
        $row = @()
        for ($x = 0; $x -lt $Width; ++$x) {
            $row += $owner
        }
        $grid += ,$row
    }
    for ($x = 0; $x -lt $Width; ++$x) { $grid[0][$x] = $neighbor }
    for ($y = 0; $y -lt $Height; ++$y) { $grid[$y][0] = $neighbor }
    $grid[3][4] = $neighbor
    $grid[3][5] = $neighbor
    $grid[4][4] = $neighbor
    $grid[4][5] = $neighbor
    for ($y = 1; $y -lt ($Height - 1); ++$y) { $grid[$y][$Width - 2] = $neighbor }

    $review = New-TilesetReviewBitmap ($Width * $TileSize) ($Height * $TileSize)
    try {
        for ($y = 0; $y -lt $Height; ++$y) {
            for ($x = 0; $x -lt $Width; ++$x) {
                $kind = $grid[$y][$x]
                $base = if ($kind -eq $owner) { $ownerBase } else { $neighborBase }
                $dest = [Drawing.Rectangle]::new($x * $TileSize, $y * $TileSize, $TileSize, $TileSize)
                Draw-TilesetAsset $review.Graphics $base $dest
            }
        }
        for ($y = 0; $y -lt $Height; ++$y) {
            for ($x = 0; $x -lt $Width; ++$x) {
                if ($grid[$y][$x] -ne $owner) {
                    continue
                }
                $mask = Get-NeighborMaskForGrid $grid $x $y $neighbor
                if ($mask -eq 0) {
                    continue
                }
                $transition = Select-TilesetTransitionAsset $transitionAssets $owner $neighbor $mask $x $y 59
                $dest = [Drawing.Rectangle]::new($x * $TileSize, $y * $TileSize, $TileSize, $TileSize)
                Draw-TilesetAsset $review.Graphics $transition $dest
            }
        }

        $outputPath = Join-Path $resolvedOutputDirectory "$owner`__to__$neighbor`__transition_stress.png"
        $review.Bitmap.Save($outputPath, [Drawing.Imaging.ImageFormat]::Png)
        $results += [pscustomobject]@{
            owner = $owner
            neighbor = $neighbor
            status = "generated"
            output_path = ConvertTo-TilesetRelativePath $outputPath
            transition_count = $pairTransitions.Count
        }
    }
    finally {
        $review.Graphics.Dispose()
        $review.Bitmap.Dispose()
    }
}

$reportPath = Join-Path $resolvedOutputDirectory "transition_stress_maps.json"
$report = [pscustomobject]@{
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    tile_size = $TileSize
    width = $Width
    height = $Height
    results = $results
}
$report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $reportPath
Write-Host "Transition stress maps wrote $reportPath"

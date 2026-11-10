param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$OutputDirectory = "",
    [int]$TileSize = 40
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

$resolvedSourceRoot = Resolve-TilesetPath $SourceRoot
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $resolvedSourceRoot "review/pairwise_contact_sheets"
}
$resolvedOutputDirectory = Resolve-TilesetPath $OutputDirectory
New-Item -ItemType Directory -Force -Path $resolvedOutputDirectory | Out-Null

$bundle = Get-TilesetV2ManifestBundle $resolvedSourceRoot
$tileAssets = @(Get-TilesetManifestAssets $bundle.Tiles)
$transitionAssets = @(Get-TilesetManifestAssets $bundle.Transitions)
$orderedPairs = ConvertTo-TilesetArray (Get-TilesetProperty $bundle.Transitions "ordered_pairs" @())
$directions = @(
    [pscustomobject]@{ name = "north"; mask = 1; dx = 0; dy = -1 },
    [pscustomobject]@{ name = "east"; mask = 2; dx = 1; dy = 0 },
    [pscustomobject]@{ name = "south"; mask = 4; dx = 0; dy = 1 },
    [pscustomobject]@{ name = "west"; mask = 8; dx = -1; dy = 0 }
)
$results = @()

foreach ($pair in $orderedPairs) {
    $owner = Get-TilesetProperty $pair "owner" ""
    $neighbor = Get-TilesetProperty $pair "neighbor" ""
    if (-not $owner -or -not $neighbor) {
        continue
    }

    $ownerBase = Select-TilesetAsset $tileAssets $owner @("base", "same_auto") 15 0 0 31
    if ($null -eq $ownerBase) {
        $ownerBase = Select-TilesetAsset $tileAssets $owner @("base") $null 0 0 31
    }
    $neighborBase = Select-TilesetAsset $tileAssets $neighbor @("base", "same_auto") 15 1 1 37
    if ($null -eq $neighborBase) {
        $neighborBase = Select-TilesetAsset $tileAssets $neighbor @("base") $null 1 1 37
    }

    $singleEdgeTransitions = @($transitionAssets | Where-Object {
        (Get-TilesetProperty $_ "asset_class" "") -eq "transition_edge" -and
        (Get-TilesetProperty $_ "owner" "") -eq $owner -and
        (Get-TilesetProperty $_ "neighbor" "") -eq $neighbor -and
        [int](Get-TilesetProperty $_ "mask" -1) -in @(1, 2, 4, 8) -and
        (Get-TilesetProperty $_ "status" "") -in @("candidate", "accepted")
    })

    if ($null -eq $ownerBase -or $null -eq $neighborBase -or $singleEdgeTransitions.Count -eq 0) {
        $results += [pscustomobject]@{
            owner = $owner
            neighbor = $neighbor
            status = "skipped"
            reason = "missing_owner_base_neighbor_base_or_single_edge_transition"
        }
        continue
    }

    $review = New-TilesetReviewBitmap (12 * $TileSize) (3 * $TileSize)
    try {
        foreach ($direction in $directions) {
            $sectionX = [array]::IndexOf($directions, $direction) * 3
            $transition = Select-TilesetTransitionAsset $transitionAssets $owner $neighbor $direction.mask $sectionX 0 41
            for ($miniY = 0; $miniY -lt 3; ++$miniY) {
                for ($miniX = 0; $miniX -lt 3; ++$miniX) {
                    $isNeighborCell = ($miniX -eq (1 + $direction.dx) -and $miniY -eq (1 + $direction.dy))
                    $asset = if ($isNeighborCell) { $neighborBase } else { $ownerBase }
                    $dest = [Drawing.Rectangle]::new(($sectionX + $miniX) * $TileSize, $miniY * $TileSize, $TileSize, $TileSize)
                    Draw-TilesetAsset $review.Graphics $asset $dest
                    if ($miniX -eq 1 -and $miniY -eq 1) {
                        Draw-TilesetAsset $review.Graphics $transition $dest
                    }
                }
            }
        }

        $outputPath = Join-Path $resolvedOutputDirectory "$owner`__to__$neighbor`__single_edge_contact_sheet.png"
        $review.Bitmap.Save($outputPath, [Drawing.Imaging.ImageFormat]::Png)
        $results += [pscustomobject]@{
            owner = $owner
            neighbor = $neighbor
            status = "generated"
            output_path = ConvertTo-TilesetRelativePath $outputPath
            transition_count = $singleEdgeTransitions.Count
        }
    }
    finally {
        $review.Graphics.Dispose()
        $review.Bitmap.Dispose()
    }
}

$reportPath = Join-Path $resolvedOutputDirectory "pairwise_transition_contact_sheets.json"
$report = [pscustomobject]@{
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    tile_size = $TileSize
    directions = @($directions | ForEach-Object { $_.name })
    results = $results
}
$report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $reportPath
Write-Host "Pairwise transition contact sheets wrote $reportPath"

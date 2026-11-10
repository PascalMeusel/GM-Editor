param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2"
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

$resolvedSourceRoot = Resolve-TilesetPath $SourceRoot
$tilesPath = Join-Path $resolvedSourceRoot "tiles.json"
$transitionsPath = Join-Path $resolvedSourceRoot "transition_pairs.json"

$tiles = Read-TilesetJson $tilesPath
$transitions = Read-TilesetJson $transitionsPath

$tileChanged = 0
$transitionChanged = 0

foreach ($asset in (Get-TilesetManifestAssets $tiles)) {
    $assetClass = Get-TilesetProperty $asset "asset_class" ""
    $owner = Get-TilesetProperty $asset "owner" ""
    $id = Get-TilesetProperty $asset "id" ""
    $variant = [int](Get-TilesetProperty $asset "variant" 0)
    if ($owner -notin @("grass_open", "snow", "water_shallow", "dirt")) {
        continue
    }

    $targetStatus = Get-TilesetProperty $asset "status" ""
    if ($assetClass -eq "base") {
        if ($id -like "*edge_locked_retry*" -and $variant -ge 209 -and $variant -le 232) {
            $targetStatus = "candidate"
        }
        else {
            $targetStatus = "needs_repaint"
        }
    }
    elseif ($assetClass -eq "same_auto") {
        $targetStatus = "needs_repaint"
    }

    if ((Get-TilesetProperty $asset "status" "") -ne $targetStatus) {
        $asset.status = $targetStatus
        ++$tileChanged
    }
}

foreach ($asset in (Get-TilesetManifestAssets $transitions)) {
    if ((Get-TilesetProperty $asset "asset_class" "") -ne "transition_edge") {
        continue
    }
    $method = Get-TilesetProperty (Get-TilesetProperty $asset "generation" $null) "method" ""
    $targetStatus = if ($method -eq "ai_material_source_smooth_transition_overlay") { "candidate" } else { "needs_repaint" }
    if ((Get-TilesetProperty $asset "status" "") -ne $targetStatus) {
        $asset.status = $targetStatus
        ++$transitionChanged
    }
    if ($targetStatus -eq "needs_repaint" -and $null -ne $asset.PSObject.Properties["validation"]) {
        $asset.validation.human_review = "needs_repaint"
    }
}

$tiles | ConvertTo-Json -Depth 14 | Set-Content -LiteralPath $tilesPath
$transitions | ConvertTo-Json -Depth 14 | Set-Content -LiteralPath $transitionsPath

$report = [pscustomobject]@{
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    source_root = ConvertTo-TilesetRelativePath $resolvedSourceRoot
    tile_status_changes = $tileChanged
    transition_status_changes = $transitionChanged
    active_base_policy = "Only edge_locked_retry v209-v232 remain candidate for first-slice base terrain."
    active_same_auto_policy = "Existing same_auto assets are needs_repaint until regenerated from the current accepted/candidate base art direction."
    active_transition_policy = "Only AI-material smooth transition overlays remain candidate; older decorative fringe/composite/blocky transition attempts are needs_repaint."
}

$reportPath = Join-Path $resolvedSourceRoot "review/status_triage_report.json"
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $reportPath

Write-Host "Status triage complete."
Write-Host "Tile status changes: $tileChanged"
Write-Host "Transition status changes: $transitionChanged"
Write-Host "Report: $reportPath"

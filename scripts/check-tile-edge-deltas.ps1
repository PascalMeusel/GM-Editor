param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$OutputPath = "",
    [int]$Band = 2,
    [double]$WarningThreshold = 18.0,
    [int]$MaxPairsPerFamily = 250
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

$resolvedSourceRoot = Resolve-TilesetPath $SourceRoot
if (-not $OutputPath) {
    $OutputPath = Join-Path $resolvedSourceRoot "review/edge_delta_reports/edge_delta_report.json"
}
$resolvedOutputPath = Resolve-TilesetPath $OutputPath
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $resolvedOutputPath) | Out-Null

$bundle = Get-TilesetV2ManifestBundle $resolvedSourceRoot
$assets = @(Get-TilesetManifestAssets $bundle.Tiles | Where-Object {
    (Get-TilesetProperty $_ "asset_class" "") -in @("base", "same_auto") -and
    (Get-TilesetProperty $_ "status" "") -in @("candidate", "accepted") -and
    -not [string]::IsNullOrWhiteSpace((Get-TilesetProperty $_ "source_path" ""))
})
$familyReports = @()
$warnings = @()

$assets | Group-Object { Get-TilesetProperty $_ "owner" "" } | ForEach-Object {
    $family = $_.Name
    $familyAssets = @($_.Group | Sort-Object { Get-TilesetProperty $_ "id" "" })
    $pairReports = @()
    $pairCount = 0

    for ($i = 0; $i -lt $familyAssets.Count; ++$i) {
        for ($j = $i + 1; $j -lt $familyAssets.Count; ++$j) {
            if ($pairCount -ge $MaxPairsPerFamily) {
                break
            }
            $left = $familyAssets[$i]
            $right = $familyAssets[$j]
            $leftPath = Get-TilesetAssetSourcePath $left
            $rightPath = Get-TilesetAssetSourcePath $right
            if (-not (Test-Path -LiteralPath $leftPath) -or -not (Test-Path -LiteralPath $rightPath)) {
                continue
            }

            $eastWestDelta = Get-TilesetColorDistance (Get-TilesetEdgeAverage $leftPath "east" $Band) (Get-TilesetEdgeAverage $rightPath "west" $Band)
            $westEastDelta = Get-TilesetColorDistance (Get-TilesetEdgeAverage $leftPath "west" $Band) (Get-TilesetEdgeAverage $rightPath "east" $Band)
            $northSouthDelta = Get-TilesetColorDistance (Get-TilesetEdgeAverage $leftPath "north" $Band) (Get-TilesetEdgeAverage $rightPath "south" $Band)
            $southNorthDelta = Get-TilesetColorDistance (Get-TilesetEdgeAverage $leftPath "south" $Band) (Get-TilesetEdgeAverage $rightPath "north" $Band)
            $maxDelta = @($eastWestDelta, $westEastDelta, $northSouthDelta, $southNorthDelta) | Measure-Object -Maximum | Select-Object -ExpandProperty Maximum
            $pairReport = [pscustomobject]@{
                left_id = Get-TilesetProperty $left "id" ""
                right_id = Get-TilesetProperty $right "id" ""
                east_to_west_delta = [Math]::Round($eastWestDelta, 2)
                west_to_east_delta = [Math]::Round($westEastDelta, 2)
                north_to_south_delta = [Math]::Round($northSouthDelta, 2)
                south_to_north_delta = [Math]::Round($southNorthDelta, 2)
                max_delta = [Math]::Round($maxDelta, 2)
                status = if ($maxDelta -gt $WarningThreshold) { "warn" } else { "ok" }
            }
            $pairReports += $pairReport
            if ($maxDelta -gt $WarningThreshold) {
                $warnings += [pscustomobject]@{
                    family = $family
                    left_id = $pairReport.left_id
                    right_id = $pairReport.right_id
                    max_delta = $pairReport.max_delta
                    issue = "same_family_edge_delta"
                }
            }
            ++$pairCount
        }
        if ($pairCount -ge $MaxPairsPerFamily) {
            break
        }
    }

    $familyReports += [pscustomobject]@{
        family = $family
        asset_count = $familyAssets.Count
        compared_pair_count = $pairReports.Count
        pairs = $pairReports
    }
}

$report = [pscustomobject]@{
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    source_root = ConvertTo-TilesetRelativePath $resolvedSourceRoot
    band = $Band
    warning_threshold = $WarningThreshold
    summary = [pscustomobject]@{
        asset_count = $assets.Count
        family_count = $familyReports.Count
        warning_count = $warnings.Count
    }
    warnings = $warnings
    families = $familyReports
}

$report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $resolvedOutputPath
Write-Host "Edge delta report wrote $resolvedOutputPath"
Write-Host "Compared families: $($familyReports.Count); warnings: $($warnings.Count)"
